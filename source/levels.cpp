// levels.cpp - 3DS level loading & brick layout rendering (modernized)
#include <3ds.h>
#include <citro2d.h>
#include <vector>
#include <string>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <unordered_map>
#include <sys/stat.h>
#include <dirent.h>
#include <algorithm>
#include "hardware.hpp"
#include "IMAGE.h"
#include "brick.hpp"
#include "levels.hpp"

namespace levels {
    // Geometry constants (match legacy main.cpp values for layout region)
    static const int BricksX=13;
    static const int BricksY=11;
    static const int NumBricks = BricksX*BricksY;    // 143 bricks
    static constexpr int LEFTSTART=28; // base left (unshifted); gameplay shift applied via render offset
    static const int TOPSTART=18;

    static const char* kSdDir = "sdmc:/ballistica";
    static const char* kLevelsSubDir = "sdmc:/ballistica/levels";
    static std::string g_activeLevelFile = "LEVELS.DAT"; // default (may be overridden by persisted config)
    static const char* kConfigPath = "sdmc:/ballistica/active.cfg"; // simple text file storing chosen DAT name
    static std::vector<std::string> g_fileList; // cached .DAT names

    struct Level {
        std::string name;
        int speed = 0;
        std::vector<uint8_t> bricks; // size NumBricks, indices into brickMap
    std::vector<uint8_t> hp;     // per-brick hit points (only used for multi-hit like T5)
    std::vector<uint8_t> origBricks; // pristine copy
    std::vector<uint8_t> origHp;     // pristine hp copy
    };

    static bool g_loaded = false;         // successfully parsed any levels
    static std::vector<Level> g_levels;    // parsed levels
    static int g_currentLevel = 0;         // index into g_levels
    static int g_renderOffsetX = 0;        // runtime horizontal render offset (gameplay mode)

    struct BrickDef { int atlasIndex; };
    // Map legacy shorthand index (enum order) to atlas image indices
    // NB(YB=1) order from support.hpp enum (starting after NB which is no brick)
    static BrickDef brickMap[] = {
        {-1}, // NB
        {IMAGE_yellow_brick_idx},
        {IMAGE_green_brick_idx},
        {IMAGE_cyan_brick_idx},
        {IMAGE_tan_brick_idx},
        {IMAGE_purple_brick_idx},
        {IMAGE_red_brick_idx},
        {IMAGE_life_brick_idx},
        {IMAGE_slow_brick_idx},
        {IMAGE_fast_brick_idx},
        {IMAGE_skull_brick_idx}, // F1 placeholder
        {IMAGE_skull_brick_idx}, // F2 placeholder
        {IMAGE_b_brick_idx},
        {IMAGE_o_brick_idx},
        {IMAGE_n_brick_idx},
        {IMAGE_u_brick_idx},
        {IMAGE_s_brick_idx},
        {IMAGE_batsmall_brick_idx},
        {IMAGE_batbig_brick_idx},
        {IMAGE_indestructible_brick_idx},
        {IMAGE_rewind_brick_idx},
        {IMAGE_reverse_brick_idx},
        {IMAGE_islow_brick_idx},
        {IMAGE_ifast_brick_idx},
        {IMAGE_another_ball_idx},
        {IMAGE_forward_brick_idx},
        {IMAGE_laser_brick_idx},
        {IMAGE_murderball_brick_idx},
        {IMAGE_bonus_brick_idx},
        {IMAGE_fivehit_brick_idx},
        {IMAGE_bomb_brick_idx},
        {IMAGE_offswitch_brick_idx},
        {IMAGE_onswitch_brick_idx},
        {IMAGE_sideslow_brick_idx},
        {IMAGE_sidehard_brick_idx}
    };

    // ---------- File helpers --------------------------------------------------
    static bool fileExists(const char* path) { struct stat st{}; return stat(path, &st)==0; }

    static void ensureOnSdmc() {
        mkdir(kSdDir, 0777); // ignore errors
        mkdir(kLevelsSubDir, 0777);
        auto copyIfMissing = [&](const char* romfsPath, const char* baseName){
            char dst[256]; snprintf(dst,sizeof dst, "%s/%s", kLevelsSubDir, baseName);
            if(fileExists(dst)) return;
            FILE* in = fopen(romfsPath, "rb");
            if(!in) {
                // Short path logging with explicit max lengths to avoid -Wformat-truncation
                char dbg[128];
                snprintf(dbg,sizeof dbg,"romfs miss '%.64s' => %.32s\n", romfsPath, baseName);
                hw_log(dbg);
                return; }
            FILE* out = fopen(dst, "wb");
            if(!out) { fclose(in); hw_log("copy fail open dst\n"); return; }
            char buf[1024]; size_t r; while((r=fread(buf,1,sizeof buf,in))>0) fwrite(buf,1,r,out);
            fclose(in); fclose(out);
            char dbg[128]; snprintf(dbg,sizeof dbg,"copied level file %s\n", baseName); hw_log(dbg);
        };
    // Enumerate romfs root & /levels for all .DAT files (no hard-coded fallbacks)
    auto scanRomfsDir = [&](const char* dirPath){
            DIR* d = opendir(dirPath);
        if(!d) { char dbg[160]; snprintf(dbg,sizeof dbg,"romfs scan: cannot open dir '%s' (fallback list used)\n", dirPath); hw_log(dbg); return; }
            struct dirent* ent;
            size_t dirLen = strlen(dirPath);
            bool endsWithSlash = dirLen>0 && dirPath[dirLen-1]=='/';
        int datFound=0;
            while((ent=readdir(d))!=nullptr) {
                const char* name = ent->d_name; if(!name) continue; size_t len=strlen(name); if(len<4) continue; const char* ext = name+len-4;
                char up[5]; for(int i=0;i<4;i++) up[i]=(char)toupper((unsigned char)ext[i]); up[4]='\0';
                if(strcmp(up,".DAT")==0) {
                    char full[320];
                    if(endsWithSlash) snprintf(full,sizeof full, "%s%s", dirPath, name); else snprintf(full,sizeof full, "%s/%s", dirPath, name);
                    copyIfMissing(full, name);
            ++datFound;
                }
            }
            closedir(d);
        char dbg[128]; snprintf(dbg,sizeof dbg,"romfs scan '%s' DAT=%d\n", dirPath, datFound); hw_log(dbg);
        };
        scanRomfsDir("romfs:/");
        scanRomfsDir("romfs:/levels"); // optional subdir
        // Clear cached list so new files appear on first open of Options
        g_fileList.clear();
    }
    // Enumerate .DAT files in levels directory
    static std::vector<std::string> listDatFiles() {
        std::vector<std::string> out; out.reserve(8);
        DIR* d = opendir(kLevelsSubDir);
        if(!d) return out;
        struct dirent* ent; while((ent = readdir(d))!=nullptr) {
            const char* name = ent->d_name; if(!name) continue; size_t len = strlen(name);
            if(len>=4) {
                const char* ext = name + len - 4;
                char up[5]; up[0]= (char)toupper(ext[0]); up[1]=(char)toupper(ext[1]); up[2]=(char)toupper(ext[2]); up[3]=(char)toupper(ext[3]); up[4]='\0';
                if(strcmp(up, ".DAT")==0) out.push_back(name);
            }
        }
        closedir(d);
        std::sort(out.begin(), out.end());
        return out;
    }
    const std::vector<std::string>& available_level_files() {
        if(g_fileList.empty()) g_fileList = listDatFiles();
        return g_fileList;
    }
    void refresh_level_files() { g_fileList = listDatFiles(); }
    static void persist_active_level_file() {
        FILE* f = fopen(kConfigPath, "wb"); if(!f) return; fprintf(f, "%s\n", g_activeLevelFile.c_str()); fclose(f);
    }
    static void load_persisted_active_file() {
        FILE* f = fopen(kConfigPath, "rb"); if(!f) return; char buf[128]; if(fscanf(f, "%127s", buf)==1) {
            // Validate it exists among available files (after ensureOnSdmc/population) before accepting
            std::string cand(buf);
            if(!cand.empty()) {
                // Defer list population until after ensureOnSdmc; list may be empty yet.
                // We'll optimistic set then later load() will fallback if missing.
                g_activeLevelFile = cand;
            }
        }
        fclose(f);
    }
    void set_active_level_file(const std::string& f) { g_activeLevelFile = f; persist_active_level_file(); }
    const std::string& get_active_level_file() { return g_activeLevelFile; }

    // ---------- Legacy shorthand mapping (2-char codes) ----------------------
    // Order must match brickMap order.
    static const char* kBrickCodes[] = {
        "NB","YB","GB","CB","TB","PB","RB","LB","SB","FB","F1","F2","B1","B2","B3","B4","B5","BS","BB","ID","RW","RE","IS","IF","AB","FO","LA","MB","BA","T5","BO","OF","ON","SS","SF"
    };
    static std::unordered_map<std::string,int> buildCodeMap() {
        std::unordered_map<std::string,int> m; m.reserve(sizeof(kBrickCodes)/sizeof(kBrickCodes[0]));
        for(size_t i=0;i<sizeof(kBrickCodes)/sizeof(kBrickCodes[0]);++i) m[kBrickCodes[i]] = (int)i;
        return m;
    }
    static const std::unordered_map<std::string,int> kCodeToIndex = buildCodeMap();

    // ---------- Fallback -----------------------------------------------------
    static void buildFallback() {
        Level L; L.name = "Fallback"; L.speed = 10; L.bricks.assign(NumBricks,0);
        for(int r=0;r<BricksY;++r) {
            int base = 1 + (r % 6); // cycle simple colored bricks
            for(int c=0;c<BricksX;++c) L.bricks[r*BricksX+c] = (uint8_t)base;
        }
    L.hp.assign(NumBricks,1);
        g_levels.push_back(L);
        g_loaded = true;
        hw_log("fallback level generated\n");
    }

    // ---------- Parser -------------------------------------------------------
    static void parseAll(FILE* f) {
        g_levels.clear();
        char token[64];
        Level cur; bool inLevel=false; int bricks=0; int lineBricks=0;
    while(fscanf(f, "%63s", token)==1) {
            if(strcmp(token,"LEVEL")==0) {
                // starting a new level: commit previous if valid
                int lvNum=0; if(fscanf(f, "%d", &lvNum)!=1) { hw_log("LEVEL missing number\n"); break; }
                if(inLevel) {
                    if((int)cur.bricks.size()==NumBricks) g_levels.push_back(cur);
                    else {
                        char dbg[64]; snprintf(dbg,sizeof dbg,"level %d incomplete (%d bricks)\n", (int)g_levels.size()+1, (int)cur.bricks.size()); hw_log(dbg);
                    }
                }
                cur = Level(); bricks=0; lineBricks=0; inLevel=true; // reset
            } else if(strcmp(token,"SPEED")==0) {
                int sp=0; fscanf(f, "%d", &sp); cur.speed=sp;
            } else if(strcmp(token,"NAME")==0) {
                // read rest of line up to newline
                int ch; std::string name; while((ch=fgetc(f))!='\n' && ch!=EOF) { if(ch=='\r') continue; name.push_back((char)ch); }
                // trim leading spaces
                size_t pos=0; while(pos<name.size() && std::isspace((unsigned char)name[pos])) ++pos; cur.name = name.substr(pos);
            } else {
                // brick code or unknown
                // Accept 2-character codes consisting of A-Z or 0-9 (e.g., YB, IF, B1, F2, T5)
                auto isCodeChar = [](unsigned char c){ return (c>='A' && c<='Z') || (c>='0' && c<='9'); };
                bool looksLikeBrickCode = (strlen(token)==2) && isCodeChar((unsigned char)token[0]) && isCodeChar((unsigned char)token[1]);
                if(inLevel && looksLikeBrickCode) {
                    auto it = kCodeToIndex.find(token);
                    int idx = (it!=kCodeToIndex.end()) ? it->second : 0;
                    if(bricks < NumBricks) {
                        if(cur.bricks.empty()) cur.bricks.reserve(NumBricks);
                        cur.bricks.push_back((uint8_t)idx); ++bricks; ++lineBricks;
                        if(cur.hp.empty()) cur.hp.reserve(NumBricks);
                        // assign hp: default 1, T5 gets 5 hits; NB/ID are non-hittable (hp=0)
                        uint8_t hpv = 1;
                        if(idx == (int)BrickType::T5) hpv = 5; else if(idx == (int)BrickType::NB) hpv = 0; else if(idx == (int)BrickType::ID) hpv = 0;
                        cur.hp.push_back(hpv);
                        if(bricks==NumBricks) {
                            if(cur.name.empty()) cur.name = "Level";
                            if(cur.speed==0) cur.speed = 10;
                            // snapshot originals
                            cur.origBricks = cur.bricks;
                            cur.origHp = cur.hp;
                        }
                    }
                } else {
                    // ignore unknown token; could log once
                }
            }
        }
        if(inLevel && (int)cur.bricks.size()==NumBricks) g_levels.push_back(cur);
        if(!g_levels.empty()) {
            g_loaded = true;
            char buf[64]; snprintf(buf,sizeof buf,"levels parsed:%d\n", (int)g_levels.size()); hw_log(buf);
        }
    }

    // ---------- Public-ish internal operations -------------------------------
    void load() {
        if(g_loaded) return;
        ensureOnSdmc();
        // Attempt to override default active file from persisted config (only first time before parsing)
        load_persisted_active_file();
    char sdPath[256]; snprintf(sdPath, sizeof(sdPath), "%s/%s", kLevelsSubDir, g_activeLevelFile.c_str());
        FILE* f = fopen(sdPath, "rb");
        if(!f) { hw_log("open sd LEVELS fail\n"); buildFallback(); return; }
        parseAll(f); fclose(f);
        if(!g_loaded) buildFallback();
    }

    void renderBricks() {
        if(!g_loaded || g_levels.empty()) return;
        const Level& L = g_levels[g_currentLevel];
        if(L.bricks.size()!=NumBricks) return;
        for(int i=0;i<NumBricks;i++) {
            uint8_t v = L.bricks[i]; if(v==0) continue; if(v >= (int)(sizeof(brickMap)/sizeof(brickMap[0]))) continue;
            int col = i % BricksX; int row = i / BricksX;
        float x = (float)(LEFTSTART + g_renderOffsetX) + col * 16; float y = TOPSTART + row * 9;
            int atlasIndex = brickMap[v].atlasIndex; if(atlasIndex<0) continue;
            // For multi-hit bricks we could choose alternate visual based on HP later.
            hw_draw_sprite(hw_image(atlasIndex), x, y);
        }
    }
    // Adjust runtime render offset
    void set_draw_offset(int off) { g_renderOffsetX = off; }
    int draw_offset() { return g_renderOffsetX; }
    int left_with_offset() { return LEFTSTART + g_renderOffsetX; }
    int levels_remaining_breakable() {
    if(g_levels.empty()) return 0;
    const auto &L = g_levels[g_currentLevel];
    int cnt=0;
    for(auto b: L.bricks) {
        if(b && b!=(int)BrickType::ID && b!=(int)BrickType::OF && b!=(int)BrickType::ON) ++cnt;
    }
    return cnt;
}

bool levels_damage_brick(int c,int r) {
    if(g_levels.empty()) return false;
    if(c<0||c>=13||r<0||r>=11) return false;
    auto &L = g_levels[g_currentLevel];
    int idx=r*13+c;
    if(idx >= (int)L.bricks.size()) return false;
    int type = L.bricks[idx];
    if(!type) return false;
    if(type == (int)BrickType::ID) return false;
    if(L.hp.size()!=L.bricks.size()) {
        L.hp.assign(L.bricks.size(),1);
        for(size_t i=0;i<L.bricks.size();++i) {
            if(L.bricks[i]==(int)BrickType::T5) L.hp[i]=5;
            else if(L.bricks[i]==0 || L.bricks[i]==(int)BrickType::ID) L.hp[i]=0;
        }
    }
    if(L.hp[idx] > 1) {
        L.hp[idx]--;
        if(L.hp[idx]==0) { L.bricks[idx]=0; return true; }
        return false;
    }
    L.bricks[idx]=0;
    L.hp[idx]=0;
    return true;
}

int levels_brick_hp(int c,int r) {
    if(g_levels.empty()) return 0;
    if(c<0||c>=13||r<0||r>=11) return 0;
    auto &L = g_levels[g_currentLevel];
    int idx=r*13+c;
    if(idx >= (int)L.bricks.size()) return 0;
    if(L.hp.size()!=L.bricks.size()) {
        return (L.bricks[idx]==(int)BrickType::T5)?5: (L.bricks[idx]==0||L.bricks[idx]==(int)BrickType::ID?0:1);
    }
    return L.hp[idx];
}

int levels_explode_bomb(int c,int r, std::vector<DestroyedBrick>* outDestroyed) {
    if(g_levels.empty()) return 0;
    if(c<0||c>=13||r<0||r>=11) return 0;
    auto &L = g_levels[g_currentLevel];
    int idx=r*13+c;
    if(idx >= (int)L.bricks.size()) return 0;
    int type = L.bricks[idx];
    if(type != (int)BrickType::BO) return 0;
    int destroyed = 0;
    std::vector<std::pair<int,int>> stack;
    stack.push_back(std::make_pair(c,r));
    while(!stack.empty()) {
        std::pair<int,int> pr = stack.back();
        stack.pop_back();
        int cc = pr.first; int rr = pr.second;
        if(cc<0||cc>=13||rr<0||rr>=11) continue;
        int i = rr*13+cc;
        if(i >= (int)L.bricks.size()) continue;
        int t = L.bricks[i];
        if(!t) continue;
        bool isBomb = (t == (int)BrickType::BO);
        if(outDestroyed) outDestroyed->push_back({cc,rr,t});
        L.bricks[i]=0;
        if(L.hp.size()==L.bricks.size()) L.hp[i]=0;
        ++destroyed;
        if(isBomb) {
            for(int dy=-1; dy<=1; ++dy) {
                for(int dx=-1; dx<=1; ++dx) {
                    if(dx||dy) stack.push_back(std::make_pair(cc+dx, rr+dy));
                }
            }
        }
    }
    return destroyed;
}

int levels_atlas_index(int rawType) {
    if(rawType <=0) return -1;
    if(rawType >= (int)(sizeof(brickMap)/sizeof(brickMap[0]))) return -1;
    return brickMap[rawType].atlasIndex;
}

void levels_reset_level(int idx) {
    if(g_levels.empty()) return;
    if(idx<0 || idx >= (int)g_levels.size()) return;
    Level &L = g_levels[idx];
    if(L.origBricks.size()==NumBricks) L.bricks = L.origBricks;
    if(L.origHp.size()==NumBricks) L.hp = L.origHp; else if(L.hp.size()==NumBricks) {
        for(int i=0;i<NumBricks;i++) {
            if(L.bricks[i]==(int)BrickType::T5) L.hp[i]=5; else if(L.bricks[i]==0 || L.bricks[i]==(int)BrickType::ID) L.hp[i]=0; else L.hp[i]=1;
        }
    }
}
void levels_snapshot_level(int idx) {
    if(levels::g_levels.empty()) return;
    if(idx<0 || idx >= (int)levels::g_levels.size()) return;
    auto &L = levels::g_levels[idx];
    if(L.bricks.size()==(size_t)levels::NumBricks) L.origBricks = L.bricks;
    if(L.hp.size()==L.bricks.size()) L.origHp = L.hp;
}

    // Editor helpers
    int edit_get_brick(int levelIndex, int col, int row) {
        if(levelIndex<0 || levelIndex >= (int)g_levels.size()) return -1;
        if(col<0||col>=BricksX||row<0||row>=BricksY) return -1;
        Level &L = g_levels[levelIndex];
        int idx = row*BricksX+col; if(idx >= (int)L.bricks.size()) return -1; return (int)L.bricks[idx];
    }
    void edit_set_brick(int levelIndex, int col, int row, int brickType) {
        if(levelIndex<0 || levelIndex >= (int)g_levels.size()) return;
        if(col<0||col>=BricksX||row<0||row>=BricksY) return;
        if(brickType < 0 || brickType >= (int)BrickType::COUNT) brickType = 0;
        Level &L = g_levels[levelIndex]; int idx=row*BricksX+col; if(idx >= (int)L.bricks.size()) return;
        L.bricks[idx] = (uint8_t)brickType;
        if(L.hp.size()==L.bricks.size()) {
            if(brickType==(int)BrickType::T5) L.hp[idx]=5; else if(brickType==0 || brickType==(int)BrickType::ID) L.hp[idx]=0; else L.hp[idx]=1;
        }
    }
    int get_speed(int levelIndex) { if(levelIndex<0||levelIndex>=(int)g_levels.size()) return 0; return g_levels[levelIndex].speed; }
    void set_speed(int levelIndex, int speed) { if(levelIndex<0||levelIndex>=(int)g_levels.size()) return; if(speed<1) speed=1; if(speed>99) speed=99; g_levels[levelIndex].speed = speed; }
    const char* get_name(int levelIndex) { if(levelIndex<0||levelIndex>=(int)g_levels.size()) return ""; return g_levels[levelIndex].name.c_str(); }
    void set_name(int levelIndex, const char* name) {
    if(levelIndex<0||levelIndex>=(int)g_levels.size()) return;
    if(!name) return;
    std::string s(name);
        if(s.size()>32) s.resize(32); // simple cap
        // trim leading spaces
        size_t p=0; while(p<s.size() && (unsigned char)s[p]<=' ') ++p; if(p>0) s = s.substr(p);
        g_levels[levelIndex].name = s;
    }
    bool save_active() {
        if(g_levels.empty()) return false;
        char path[256]; snprintf(path,sizeof path, "%s/%s", kLevelsSubDir, g_activeLevelFile.c_str());
        FILE* f = fopen(path, "wb"); if(!f) return false;
        for(size_t li=0; li<g_levels.size(); ++li) {
            const Level &L = g_levels[li];
            fprintf(f, "LEVEL %zu\n", li+1);
            fprintf(f, "SPEED %d\n", L.speed);
            fprintf(f, "NAME %s\n", L.name.c_str());
            // bricks: output 13 codes per line for readability
            for(int r=0;r<BricksY;r++) {
                for(int c=0;c<BricksX;c++) {
                    int idx = r*BricksX+c; int b = (idx<(int)L.bricks.size())? L.bricks[idx]:0;
                    const char* code = (b>=0 && b < (int)(sizeof(kBrickCodes)/sizeof(kBrickCodes[0]))) ? kBrickCodes[b] : "NB";
                    fprintf(f, "%s", code);
                    if(c<BricksX-1) fputc(' ', f);
                }
                fputc('\n', f);
            }
        }
        fclose(f);
        return true;
    }

} // namespace levels

// Public simple C interface used by game.cpp
void levels_load() { levels::load(); }
void levels_render() { levels::renderBricks(); }
int levels_count() { return (int)levels::g_levels.size(); }
int levels_current() { return levels::g_currentLevel; }
bool levels_set_current(int idx) { if(idx>=0 && idx < (int)levels::g_levels.size()) { levels::g_currentLevel = idx; return true; } return false; }
int levels_grid_width() { return 13; }
int levels_grid_height() { return 11; }
int levels_left() { return levels::left_with_offset(); }
int levels_top() { return 18; }
int levels_brick_width() { return 16; }
int levels_brick_height() { return 9; }
int levels_brick_at(int c,int r) { if(levels::g_levels.empty()) return -1; if(c<0||c>=13||r<0||r>=11) return -1; const auto &L = levels::g_levels[levels::g_currentLevel]; int idx=r*13+c; if(idx >= (int)L.bricks.size()) return -1; return (int)L.bricks[idx]; }
void levels_remove_brick(int c,int r) { if(levels::g_levels.empty()) return; if(c<0||c>=13||r<0||r>=11) return; auto &L = levels::g_levels[levels::g_currentLevel]; int idx=r*13+c; if(idx >= (int)L.bricks.size()) return; L.bricks[idx]=0; if(L.hp.size()==L.bricks.size()) L.hp[idx]=0; }
int levels_remaining_breakable() { return levels::levels_remaining_breakable(); }
bool levels_damage_brick(int c,int r) { return levels::levels_damage_brick(c,r); }
int levels_brick_hp(int c,int r) { return levels::levels_brick_hp(c,r); }
int levels_explode_bomb(int c,int r, std::vector<DestroyedBrick>* outDestroyed) { return levels::levels_explode_bomb(c,r,outDestroyed); }
int levels_atlas_index(int rawType) { return levels::levels_atlas_index(rawType); }
void levels_reset_level(int idx) { levels::levels_reset_level(idx); }
void levels_snapshot_level(int idx) { levels::levels_snapshot_level(idx); }
void levels_set_draw_offset(int off) { levels::set_draw_offset(off); }
int levels_get_draw_offset() { return levels::draw_offset(); }
// New selection APIs
const std::vector<std::string>& levels_available_files() { return levels::available_level_files(); }
void levels_refresh_files() { levels::refresh_level_files(); }
void levels_set_active_file(const char* f) { if(f) levels::set_active_level_file(f); }
const char* levels_get_active_file() { return levels::get_active_level_file().c_str(); }
void levels_reload_active() { using namespace levels; g_loaded=false; g_levels.clear(); load(); }
bool levels_duplicate_active(const char* newBase) {
    if(!newBase || !*newBase) return false;
    // Sanitize: uppercase, strip invalid, max 8 chars
    char base[9]; int bi=0; for(const char* p=newBase; *p && bi<8; ++p) {
        char c=*p; if(c>='a'&&c<='z') c = (char)(c-'a'+'A');
        if(!((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_')) continue; // allow alnum underscore
        base[bi++]=c;
    }
    if(bi==0) return false;
    base[bi]='\0';
    // Source file path
    const char* active = levels_get_active_file(); if(!active) return false;
    char src[256]; snprintf(src,sizeof src,"%s/%s", "sdmc:/ballistica/levels", active);
    char dst[256]; snprintf(dst,sizeof dst,"%s/%s.DAT", "sdmc:/ballistica/levels", base);
    // Prevent overwrite
    struct stat st{}; if(stat(dst,&st)==0) return false;
    FILE* in = fopen(src,"rb"); if(!in) return false;
    FILE* out = fopen(dst,"wb"); if(!out) { fclose(in); return false; }
    char buf[1024]; size_t r; while((r=fread(buf,1,sizeof buf,in))>0) fwrite(buf,1,r,out);
    fclose(in); fclose(out);
    levels_refresh_files();
    return true;
}
// Editor facade
int  levels_edit_get_brick(int levelIndex, int col, int row) { return levels::edit_get_brick(levelIndex,col,row); }
void levels_edit_set_brick(int levelIndex, int col, int row, int brickType) { levels::edit_set_brick(levelIndex,col,row,brickType); }
int  levels_get_speed(int levelIndex) { return levels::get_speed(levelIndex); }
void levels_set_speed(int levelIndex, int speed) { levels::set_speed(levelIndex,speed); }
const char* levels_get_name(int levelIndex) { return levels::get_name(levelIndex); }
void levels_set_name(int levelIndex, const char* name) { levels::set_name(levelIndex,name); }
bool levels_save_active() { return levels::save_active(); }
void levels_persist_active_file() { levels::persist_active_level_file(); }