#include <3ds.h>
#include <citro2d.h>

int main(int argc, char* argv[])
{
    // Initialize graphics and citro2d
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    // Create two screens
    C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    // Load textures (converted with tex3ds: tex3ds top.png -o top.t3x)
    C2D_SpriteSheet spritesTop  = C2D_SpriteSheetLoad("romfs:/top.t3x");
    C2D_SpriteSheet spritesBottom = C2D_SpriteSheetLoad("romfs:/bottom.t3x");

    if(!spritesTop || !spritesBottom) svcBreak(USERBREAK_PANIC);

    C2D_Image imgTop    = C2D_SpriteSheetGetImage(spritesTop, 0);
    C2D_Image imgBottom = C2D_SpriteSheetGetImage(spritesBottom, 0);

    while (aptMainLoop())
    {
        hidScanInput();
        u32 kDown = hidKeysDown();
        if (kDown & KEY_START) break;   // Exit with START

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        // --- Draw top screen ---
        C2D_TargetClear(top, C2D_Color32(0,0,0,255));
        C2D_SceneBegin(top);
        C2D_DrawImageAt(imgTop, 100, 80, 0.0f, NULL, 1.0f, 1.0f);

        // --- Draw bottom screen ---
        C2D_TargetClear(bottom, C2D_Color32(0,0,64,255));
        C2D_SceneBegin(bottom);
        C2D_DrawImageAt(imgBottom, 60, 60, 0.0f, NULL, 1.0f, 1.0f);

        C3D_FrameEnd(0);
    }

    // Cleanup
    C2D_SpriteSheetFree(spritesTop);
    C2D_SpriteSheetFree(spritesBottom);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
