// 3DS platform implementation (citro2d) replacing legacy DOS VGA routines.
// Bottom screen (320x240) rendering only.

#include "hardware.hpp"
#ifdef PLATFORM_3DS
#include <3ds.h>
#include <citro2d.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <string>

#include "IMAGE_t3x.h"
#include "IMAGE.h"
#include "BREAK_t3x.h"
#include "BREAK.h"
#include "TITLE_t3x.h"
#include "TITLE.h"
#include "INSTRUCT_t3x.h"
#include "INSTRUCT.h"
#include "DESIGNER_t3x.h"
#include "DESIGNER.h"
#include "HIGH_t3x.h"
#include "HIGH.h"
#include "TOUCH_t3x.h"
#include "TOUCH.h"
#include "OPTIONS_t3x.h"
#include "OPTIONS.h"

#include "sprite_indexes/image_indices.h"

namespace {
    C3D_RenderTarget* g_bottom = nullptr;
    C3D_RenderTarget* g_top = nullptr;
    C2D_SpriteSheet g_sheetImage = nullptr;
    C2D_SpriteSheet g_sheetBreak = nullptr;
    C2D_SpriteSheet g_sheetTitle = nullptr;
    C2D_SpriteSheet g_sheetInstruct = nullptr;
    C2D_SpriteSheet g_sheetDesigner = nullptr; // placeholder for future
    C2D_SpriteSheet g_sheetHigh = nullptr;
    C2D_SpriteSheet g_sheetTouch = nullptr;
    C2D_SpriteSheet g_sheetOptions = nullptr;
    // Tiny log ring buffer
    std::vector<std::string> g_logs;
    const size_t kMaxLogLines = 64;

    // 5x6 pixel bitmap font (uppercase + digits + some punctuation)
    // Each row uses low 5 bits of a byte.
    struct Glyph { char c; uint8_t rows[6]; };
    static const Glyph kGlyphs[] = {
        {'A',{0x0E,0x11,0x1F,0x11,0x11,0x00}},
        {'B',{0x1E,0x11,0x1E,0x11,0x1E,0x00}},
        {'C',{0x0E,0x11,0x10,0x11,0x0E,0x00}},
        {'D',{0x1E,0x11,0x11,0x11,0x1E,0x00}},
        {'E',{0x1F,0x10,0x1E,0x10,0x1F,0x00}},
        {'F',{0x1F,0x10,0x1E,0x10,0x10,0x00}},
        {'G',{0x0F,0x10,0x13,0x11,0x0F,0x00}},
        {'H',{0x11,0x11,0x1F,0x11,0x11,0x00}},
        {'I',{0x1F,0x04,0x04,0x04,0x1F,0x00}},
        {'J',{0x01,0x01,0x01,0x11,0x0E,0x00}},
        {'K',{0x11,0x12,0x1C,0x12,0x11,0x00}},
        {'L',{0x10,0x10,0x10,0x10,0x1F,0x00}},
        {'M',{0x11,0x1B,0x15,0x11,0x11,0x00}},
        {'N',{0x11,0x19,0x15,0x13,0x11,0x00}},
        {'O',{0x0E,0x11,0x11,0x11,0x0E,0x00}},
        {'P',{0x1E,0x11,0x1E,0x10,0x10,0x00}},
        {'Q',{0x0E,0x11,0x11,0x15,0x0E,0x01}},
        {'R',{0x1E,0x11,0x1E,0x12,0x11,0x00}},
        {'S',{0x0F,0x10,0x0E,0x01,0x1E,0x00}},
        {'T',{0x1F,0x04,0x04,0x04,0x04,0x00}},
        {'U',{0x11,0x11,0x11,0x11,0x0E,0x00}},
        {'V',{0x11,0x11,0x11,0x0A,0x04,0x00}},
        {'W',{0x11,0x11,0x15,0x1B,0x11,0x00}},
        {'X',{0x11,0x0A,0x04,0x0A,0x11,0x00}},
        {'Y',{0x11,0x0A,0x04,0x04,0x04,0x00}},
        {'Z',{0x1F,0x02,0x04,0x08,0x1F,0x00}},
        {'0',{0x0E,0x13,0x15,0x19,0x0E,0x00}},
        {'1',{0x04,0x0C,0x04,0x04,0x0E,0x00}},
        {'2',{0x0E,0x11,0x02,0x04,0x1F,0x00}},
        {'3',{0x1F,0x02,0x04,0x02,0x1F,0x00}},
        {'4',{0x02,0x06,0x0A,0x1F,0x02,0x00}},
        {'5',{0x1F,0x10,0x1E,0x01,0x1E,0x00}},
        {'6',{0x06,0x08,0x1E,0x11,0x0E,0x00}},
        {'7',{0x1F,0x01,0x02,0x04,0x08,0x00}},
        {'8',{0x0E,0x11,0x0E,0x11,0x0E,0x00}},
        {'9',{0x0E,0x11,0x0F,0x01,0x06,0x00}},
        {':',{0x00,0x04,0x00,0x04,0x00,0x00}},
        {'.',{0x00,0x00,0x00,0x00,0x04,0x00}},
        {'-',{0x00,0x00,0x0E,0x00,0x00,0x00}},
        {'_',{0x00,0x00,0x00,0x00,0x1F,0x00}},
    {'/',{0x01,0x02,0x04,0x08,0x10,0x00}},
        {' ' ,{0x00,0x00,0x00,0x00,0x00,0x00}},
    };

    const Glyph* findGlyph(char c) {
        if(c>='a' && c<='z') c = (char)(c - 'a' + 'A');
        for(const auto &g : kGlyphs) if(g.c==c) return &g;
        return &kGlyphs[sizeof(kGlyphs)/sizeof(kGlyphs[0]) - 1]; // space fallback
    }


    void drawGlyphString(int x,int y,const char* s, uint32_t rgba) {
        uint8_t r=(rgba>>24)&0xFF,g=(rgba>>16)&0xFF,b=(rgba>>8)&0xFF,a=rgba&0xFF;
        for(const char* p=s; *p; ++p) {
            const Glyph* gptr = findGlyph(*p);
            for(int ry=0; ry<6; ++ry) {
                uint8_t row = gptr->rows[ry];
                for(int rx=0; rx<5; ++rx) if(row & (1 << (4-rx)))
                    C2D_DrawRectSolid(x+rx, y+ry, 0, 1,1, C2D_Color32(r,g,b,a));
            }
            x += 6; if(x > 320-6) break; // bottom width 320
        }
    }
}

bool hw_init() {
    gfxInitDefault();
    hw_log("hw_init start\n");
    romfsInit();
    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) { hw_log("C3D_Init FAILED\n"); return false; }
    if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) { hw_log("C2D_Init FAILED\n"); return false; }
    C2D_Prepare();
    g_bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    g_top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    if(!g_bottom) return false;
    g_sheetImage = C2D_SpriteSheetLoadFromMem(IMAGE_t3x, IMAGE_t3x_size);
    g_sheetBreak = C2D_SpriteSheetLoadFromMem(BREAK_t3x, BREAK_t3x_size);
    g_sheetTitle = C2D_SpriteSheetLoadFromMem(TITLE_t3x, TITLE_t3x_size);
    g_sheetInstruct = C2D_SpriteSheetLoadFromMem(INSTRUCT_t3x, INSTRUCT_t3x_size);
    g_sheetDesigner = C2D_SpriteSheetLoadFromMem(DESIGNER_t3x, DESIGNER_t3x_size);
    g_sheetHigh = C2D_SpriteSheetLoadFromMem(HIGH_t3x, HIGH_t3x_size);
    g_sheetTouch = C2D_SpriteSheetLoadFromMem(TOUCH_t3x, TOUCH_t3x_size);
    g_sheetOptions = C2D_SpriteSheetLoadFromMem(OPTIONS_t3x, OPTIONS_t3x_size);
    if(!g_sheetImage) hw_log("Failed IMAGE\n"); else hw_log("Loaded IMAGE\n");
    if(!g_sheetBreak) hw_log("Failed BREAK\n");
    if(!g_sheetTitle) hw_log("Failed TITLE\n");
    if(!g_sheetInstruct) hw_log("Failed INSTRUCT\n");
    if(!g_sheetDesigner) hw_log("Failed DESIGNER\n");
    if(!g_sheetHigh) hw_log("Failed HIGH\n");
    if(!g_sheetTouch) hw_log("Failed TOUCH\n");
    if(!g_sheetOptions) hw_log("Failed OPTIONS\n");
    return g_sheetImage != nullptr;
}

void hw_shutdown() {
    romfsExit();
    if(g_sheetDesigner) { C2D_SpriteSheetFree(g_sheetDesigner); g_sheetDesigner=nullptr; }
    if(g_sheetHigh) { C2D_SpriteSheetFree(g_sheetHigh); g_sheetHigh=nullptr; }
    if(g_sheetTouch) { C2D_SpriteSheetFree(g_sheetTouch); g_sheetTouch=nullptr; }
    if(g_sheetOptions) { C2D_SpriteSheetFree(g_sheetOptions); g_sheetOptions=nullptr; }
    if(g_sheetInstruct) { C2D_SpriteSheetFree(g_sheetInstruct); g_sheetInstruct=nullptr; }
    if(g_sheetTitle) { C2D_SpriteSheetFree(g_sheetTitle); g_sheetTitle=nullptr; }
    if(g_sheetBreak) { C2D_SpriteSheetFree(g_sheetBreak); g_sheetBreak=nullptr; }
    if(g_sheetImage) { C2D_SpriteSheetFree(g_sheetImage); g_sheetImage=nullptr; }
    // Screen target is owned by citro2d; no explicit delete needed for default screen
    C2D_Fini();
    C3D_Fini();
    gfxExit();
}

void hw_poll_input(InputState& out) {
    hidScanInput();
    u32 kHeld = hidKeysHeld();
    u32 kDown = hidKeysDown();
    touchPosition tp{};
    out.touching = (kHeld & KEY_TOUCH) != 0;
    out.touchPressed = (kDown & KEY_TOUCH) != 0;
    if(out.touching) { hidTouchRead(&tp); out.stylusX = tp.px; out.stylusY = tp.py; }
    else { out.stylusX = out.stylusY = -1; }
    out.fireHeld = (kHeld & KEY_DUP) != 0;
    out.startPressed = (kDown & KEY_START) != 0;
    out.selectPressed = (kDown & KEY_SELECT) != 0;
    out.aPressed = (kDown & KEY_A) != 0;
    out.bPressed = (kDown & KEY_B) != 0;
    out.xPressed = (kDown & KEY_X) != 0;
    out.levelPrevPressed = (kDown & KEY_L) != 0;
    out.levelNextPressed = (kDown & KEY_R) != 0;
}

void hw_begin_frame() {
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    // We'll leave clearing/drawing order to higher level now.
    C2D_TargetClear(g_top, C2D_Color32(0,0,0,255));
    C2D_TargetClear(g_bottom, C2D_Color32(0,0,0,255));
}
void hw_end_frame() { C3D_FrameEnd(0); }

void hw_draw_sprite(C2D_Image img, float x, float y, float z, float sx, float sy) {
    C2D_DrawImageAt(img, x, y, z, nullptr, sx, sy);
}

void hw_draw_text(int x,int y,const char* text, uint32_t rgba) { drawGlyphString(x,y,text,rgba); }

void hw_draw_text_scaled(int x,int y,const char* text, uint32_t rgba, float scale) {
    if(scale <= 1.01f) { hw_draw_text(x,y,text,rgba); return; }
    float cursorX = (float)x;
    uint8_t r=(rgba>>24)&0xFF,g=(rgba>>16)&0xFF,b=(rgba>>8)&0xFF,a=rgba&0xFF;
    for(const char* p=text; *p; ++p) {
        char c = *p;
        if(c=='\n') { y += (int)std::ceil(6*scale + scale); cursorX = (float)x; continue; }
        const Glyph* glyph = findGlyph(c);
        for(int ry=0; ry<6; ++ry) {
            uint8_t row = glyph->rows[ry];
            for(int rx=0; rx<5; ++rx) if(row & (1<<(4-rx))) {
                float px = cursorX + rx*scale;
                float py = (float)y + ry*scale;
                C2D_DrawRectSolid(px, py, 0, scale, scale, C2D_Color32(r,g,b,a));
            }
        }
        cursorX += 6*scale; if(cursorX > 400 - 6*scale) break;
    }
}

void hw_draw_logs(int x,int y,int maxPixelsY) {
    // Render logs onto whichever target is current (caller sets scene)
    const int lineH=7; int maxLines = maxPixelsY / lineH; if(maxLines<=0) return;
    int start = (int)g_logs.size() - maxLines; if(start<0) start=0; int yy=y;
    for(size_t i=start;i<g_logs.size();++i) {
        int xx=x;
        for(char c : g_logs[i]) {
            const Glyph* g = findGlyph(c);
            for(int ry=0; ry<6; ++ry) {
                uint8_t row = g->rows[ry];
                for(int rx=0; rx<5; ++rx) if(row & (1<<(4-rx))) C2D_DrawRectSolid(xx+rx, yy+ry, 0,1,1,C2D_Color32(180,180,180,255));
            }
            xx += 6; if(xx > 320-6) break;
        }
        yy += lineH; if(yy + lineH > y + maxPixelsY) break;
    }
}

void hw_set_top() {
    if(g_top) C2D_SceneBegin(g_top);
}
void hw_set_bottom() {
    if(g_bottom) C2D_SceneBegin(g_bottom);
}

C2D_Image hw_image(int index) {
    if(!g_sheetImage) return C2D_Image{};
    return C2D_SpriteSheetGetImage(g_sheetImage, index);
}

bool hw_sheet_loaded(HwSheet sheet) {
    switch(sheet) {
        case HwSheet::Image: return g_sheetImage;
        case HwSheet::Break: return g_sheetBreak;
        case HwSheet::Title: return g_sheetTitle;
    case HwSheet::High: return g_sheetHigh;
        case HwSheet::Instruct: return g_sheetInstruct;
        case HwSheet::Designer: return g_sheetDesigner; // currently null
    case HwSheet::Touch: return g_sheetTouch;
    case HwSheet::Options: return g_sheetOptions;
    }
    return false;
}

C2D_Image hw_image_from(HwSheet sheet, int index) {
    C2D_SpriteSheet s = nullptr;
    switch(sheet) {
        case HwSheet::Image: s = g_sheetImage; break;
        case HwSheet::Break: s = g_sheetBreak; break;
        case HwSheet::Title: s = g_sheetTitle; break;
    case HwSheet::High: s = g_sheetHigh; break;
        case HwSheet::Instruct: s = g_sheetInstruct; break;
        case HwSheet::Designer: s = g_sheetDesigner; break;
    case HwSheet::Touch: s = g_sheetTouch; break;
    case HwSheet::Options: s = g_sheetOptions; break;
    }
    if(!s) return C2D_Image{};
    return C2D_SpriteSheetGetImage(s, index);
}

void hw_log(const char* msg) {
    if(!msg) return;
    // Split into lines, coalesce immediate repeats, store, and forward to emulator (svcOutputDebugString) + stderr.
    const char* p = msg;
    static std::string lastLine;
    static int repeatCount = 0;
    auto commitRepeat = [&]() {
        if(repeatCount > 1 && !g_logs.empty()) {
            g_logs.back() = lastLine + " (x" + std::to_string(repeatCount) + ")";
            // Also emit one summary line to emulator
            std::string summary = lastLine + " (x" + std::to_string(repeatCount) + ")\n";
            svcOutputDebugString(summary.c_str(), (s32)summary.size());
            fprintf(stderr, "%s", summary.c_str());
        }
        repeatCount = 0;
    };
    while(*p) {
        const char* start = p;
        while(*p && *p!='\n') ++p;
        std::string line(start, p-start);
        if(!line.empty()) {
            if(line == lastLine) {
                ++repeatCount;
            } else {
                commitRepeat();
                lastLine = line;
                repeatCount = 1;
                g_logs.push_back(line);
                if(g_logs.size() > kMaxLogLines) g_logs.erase(g_logs.begin(), g_logs.begin() + (g_logs.size()-kMaxLogLines));
                std::string outLine = line + "\n";
                svcOutputDebugString(outLine.c_str(), (s32)outLine.size());
                fprintf(stderr, "%s", outLine.c_str());
            }
        }
        if(*p=='\n') ++p; // skip newline
    }
    // Do not flush repeats yet if message could be continued in next call.
}

#endif // PLATFORM_3DS
