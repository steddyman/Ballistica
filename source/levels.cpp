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
#include "hardware.hpp"
#include "IMAGE.h"

namespace levels {
    // Geometry constants (match legacy main.cpp values for layout region)
    static const int BricksX=13;
    static const int BricksY=11;
    static const int NumBricks = BricksX*BricksY;    // 143 bricks
    static const int LEFTSTART=28;
    static const int TOPSTART=18;

    static const char* kSdDir = "sdmc:/murderball";
    static const char* kLevelFile = "LEVELS.DAT";

    struct Level {
        std::string name;
        int speed = 0;
        std::vector<uint8_t> bricks; // size NumBricks, indices into brickMap
    };

    static bool g_loaded = false;         // successfully parsed any levels
    static std::vector<Level> g_levels;    // parsed levels
    static int g_currentLevel = 0;         // index into g_levels

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
        char sdPath[256]; snprintf(sdPath, sizeof(sdPath), "%s/%s", kSdDir, kLevelFile);
        if(fileExists(sdPath)) return; // already present
        FILE* in = fopen("romfs:/LEVELS.DAT", "rb");
        if(!in) { hw_log("no romfs LEVELS.DAT\n"); return; }
        FILE* out = fopen(sdPath, "wb");
        if(!out) { fclose(in); hw_log("cant create sd LEVELS.DAT\n"); return; }
        char buf[512]; size_t r; while((r=fread(buf,1,sizeof buf,in))>0) fwrite(buf,1,r,out);
        fclose(in); fclose(out); hw_log("copied LEVELS.DAT -> sdmc\n");
    }

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
                if(strlen(token)==2 && std::isupper((unsigned char)token[0]) && std::isupper((unsigned char)token[1])) {
                    auto it = kCodeToIndex.find(token);
                    int idx = (it!=kCodeToIndex.end()) ? it->second : 0;
                    if(bricks < NumBricks) {
                        if(cur.bricks.empty()) cur.bricks.reserve(NumBricks);
                        cur.bricks.push_back((uint8_t)idx); ++bricks; ++lineBricks;
                        if(bricks==NumBricks) {
                            if(cur.name.empty()) {
                                cur.name = "Level";
                            }
                            if(cur.speed==0) {
                                cur.speed = 10;
                            }
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
        char sdPath[256]; snprintf(sdPath, sizeof(sdPath), "%s/%s", kSdDir, kLevelFile);
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
            float x = LEFTSTART + col * 16; float y = TOPSTART + row * 9;
            int atlasIndex = brickMap[v].atlasIndex; if(atlasIndex<0) continue;
            hw_draw_sprite(hw_image(atlasIndex), x, y);
        }
    }
}

// Public simple C interface used by game.cpp
void levels_load() { levels::load(); }
void levels_render() { levels::renderBricks(); }