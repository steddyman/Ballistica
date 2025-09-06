// 3DS platform implementation (citro2d) replacing legacy DOS VGA routines.
// Bottom screen (320x240) rendering only.

#include "hardware.hpp"
#ifdef PLATFORM_3DS
#include <3ds.h>
#include <citro2d.h>

#include "IMAGE_t3x.h" // binary data symbols
#include "IMAGE.h"     // index definitions

#include "sprite_indexes/image_indices.h"

namespace {
    C3D_RenderTarget* g_bottom = nullptr;
    C2D_SpriteSheet g_sheetImage = nullptr;
    bool g_consoleInit = false;
}

bool hw_init() {
    gfxInitDefault();
    // init simple text console on top screen for debugging
    consoleInit(GFX_TOP, nullptr); g_consoleInit = true; hw_log("hw_init start\n");
    if (C3D_Init(C3D_DEFAULT_CMDBUF_SIZE) != 0) return false;
    if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) return false;
    C2D_Prepare();
    g_bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    if(!g_bottom) return false;
    g_sheetImage = C2D_SpriteSheetLoadFromMem(IMAGE_t3x, IMAGE_t3x_size);
    if(!g_sheetImage) hw_log("Failed to load IMAGE sheet\n"); else hw_log("Loaded IMAGE sheet\n");
    return g_sheetImage != nullptr;
}

void hw_shutdown() {
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
    if(out.touching) { hidTouchRead(&tp); out.stylusX = tp.px; out.stylusY = tp.py; }
    else { out.stylusX = out.stylusY = -1; }
    out.fireHeld = (kHeld & KEY_DUP) != 0;
    out.startPressed = (kDown & KEY_START) != 0;
}

void hw_begin_frame() {
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TargetClear(g_bottom, C2D_Color32(0,0,0,255));
    C2D_SceneBegin(g_bottom);
}
void hw_end_frame() { C3D_FrameEnd(0); }

void hw_draw_sprite(C2D_Image img, float x, float y, float z, float sx, float sy) {
    C2D_DrawImageAt(img, x, y, z, nullptr, sx, sy);
}

C2D_Image hw_image(int index) {
    if(!g_sheetImage) return C2D_Image{};
    return C2D_SpriteSheetGetImage(g_sheetImage, index);
}

void hw_log(const char* msg) {
    if(!g_consoleInit || !msg) return;
    printf("%s", msg);
}

#endif // PLATFORM_3DS
