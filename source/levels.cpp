// levels.cpp - minimal 3DS level loading & brick layout rendering
#include <3ds.h>
#include <citro2d.h>
#include <vector>
#include <string>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include "hardware.hpp"
#include "IMAGE.h"

namespace levels {
    static const char* kSdDir = "sdmc:/murderball";
    static const char* kLevelFile = "LEVELS.DAT";
    static bool g_loaded = false;
    static int g_maxLevel = 0;
    static std::vector<uint8_t> g_levelData; // level 1 only for now (NumBricks=143) -> 13*11
    static const int BricksX=13;
    static const int BricksY=11;
    static const int NumBricks = BricksX*BricksY;
    static const int LEFTSTART=28;
    static const int TOPSTART=18;

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

    static bool fileExists(const char* path) {
        struct stat st{}; return stat(path, &st) == 0; }

    static void ensureOnSdmc() {
        mkdir(kSdDir, 0777); // ignore errors
        char sdPath[256]; snprintf(sdPath, sizeof(sdPath), "%s/%s", kSdDir, kLevelFile);
        if(fileExists(sdPath)) return; // already copied
        FILE* in = fopen("romfs:/LEVELS.DAT", "rb");
        if(!in) { hw_log("no romfs LEVELS.DAT\n"); return; }
        FILE* out = fopen(sdPath, "wb");
        if(!out) { fclose(in); hw_log("cant create sd LEVELS.DAT\n"); return; }
        char buf[512]; size_t r; while((r=fread(buf,1,sizeof buf,in))>0) fwrite(buf,1,r,out);
        fclose(in); fclose(out); hw_log("copied LEVELS.DAT -> sdmc\n");
    }

    // Create a simple built-in default pattern (only used if no file present anywhere).
    static void buildFallbackLevel() {
        g_levelData.assign(NumBricks, 0);
        // Color rows: Y,G,C,T,P,R repeating (indices 1..6 in our simplified brickMap)
        for(int row=0; row<BricksY; ++row) {
            int colorIdx = 1 + (row % 6); // cycle through first 6 colored bricks
            for(int col=0; col<BricksX; ++col) {
                int i = row * BricksX + col;
                g_levelData[i] = (uint8_t)colorIdx;
            }
        }
        g_loaded = true; g_maxLevel = 1; hw_log("fallback level generated\n");
    }

    static void parseLevel1(FILE* f) {
        // crude parser: seek SPEED/NAME then read 143 tokens of brick shorthand (2+ chars)
        g_levelData.assign(NumBricks, 0);
        char word[32]; int bricksRead=0;
        while(fscanf(f, "%31s", word)==1) {
            if(strcmp(word, "LEVEL")==0) { int lv; fscanf(f, "%d", &lv); if(lv==1) { bricksRead=0; } }
            else if(strcmp(word, "SPEED")==0) { int sp; fscanf(f, "%d", &sp); }
            else if(strcmp(word, "NAME")==0) { fgets(word, sizeof word, f); }
            else if(isalpha((unsigned char)word[0])) {
                // treat as brick shorthand
                if(bricksRead < NumBricks) {
                    // map shorthand by first letter group (simplified)
                    int idx=0;
                    // Very naive mapping: single letters to known brick indices (expand later)
                    switch(word[0]) {
                        case 'Y': idx=1; break; // yellow
                        case 'G': idx=2; break; // green
                        case 'C': idx=3; break; // cyan
                        case 'T': idx=4; break; // tan
                        case 'P': idx=5; break; // purple
                        case 'R': idx=6; break; // red
                        case 'L': idx=7; break; // life or laser (ambiguous)
                        case 'S': idx=8; break; // slow
                        case 'F': idx=9; break; // fast / fivehit / forward (ambiguous)
                        case 'B': idx=12; break; // B letter
                    }
                    g_levelData[bricksRead++] = (uint8_t)idx;
                    if(bricksRead==NumBricks) break;
                }
            }
        }
        g_loaded = true; g_maxLevel = 1; hw_log("parsed level1\n");
    }

    void load() {
        if(g_loaded) return;
        ensureOnSdmc();
        char sdPath[256]; snprintf(sdPath, sizeof(sdPath), "%s/%s", kSdDir, kLevelFile);
        FILE* f = fopen(sdPath, "rb");
    if(!f) { hw_log("open sd LEVELS fail\n"); buildFallbackLevel(); return; }
        parseLevel1(f);
        fclose(f);
    }

    void renderBricks() {
        if(!g_loaded) return;
        for(int i=0;i<NumBricks;i++) {
            uint8_t v = g_levelData[i]; if(v==0) continue; if(v >= (int)(sizeof(brickMap)/sizeof(brickMap[0]))) continue;
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