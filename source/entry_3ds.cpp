#include <3ds.h>
#include "hardware.hpp"
#include "IMAGE.h"
#include "IMAGE_t3x.h"
#include "sprite_indexes/image_indices.h"
#include "game.hpp"
#include "sound.hpp"
#include "options.hpp"

int main(int argc, char** argv) {
    if(!hw_init()) return -1;
    // Load persisted options before audio starts
    options::load_settings();
    sound::init();
    // Preload common SFX to prevent on-hit stutter
    sound::preload_sfx("ball-brick");
    sound::preload_sfx("ball-bat");
    sound::preload_sfx("barrier-hit");
    sound::preload_sfx("explosion");
    sound::preload_sfx("hard-explode");
    // Start background music (romfs:/audio/music.wav) if enabled, looped at 80% volume
    if (options::is_music_enabled()) {
        sound::play_music("music", /*loop=*/true, /*volume=*/0.8f, /*relativePath=*/true);
    }
    game_init();
    u32 frame=0;
    bool showTopLogs=false;
    bool showBottomLogs=false;

    while (aptMainLoop()) {
        InputState in; hw_poll_input(in);
        // Exit if game layer requested (X on title or touch EXIT) or START+SELECT chord anywhere as hard quit
        if(exit_requested() || (in.startPressed && in.selectPressed)) break;
    game_update(in);
    sound::update();
        // Toggle log overlays: exact combo L+R+Up or L+R+Down (edge on Up/Down).
        if(in.lHeld && in.rHeld) {
            if(in.dpadUpPressed) showTopLogs = !showTopLogs;
            if(in.dpadDownPressed) showBottomLogs = !showBottomLogs;
        }
        hw_begin_frame();
        GameMode gm = game_mode();
        // Dedicated handling: Editor and Options both own the bottom screen completely.
        if(gm == GameMode::Options) {
            // Top: simple dark backdrop (could show rotating title sequence later if desired)
            hw_set_top();
            C2D_DrawRectSolid(0,0,0,400,240,C2D_Color32(0,0,0,255));
            hw_draw_text(8,8,"Options",0xFFFFFFFF);
            if(showTopLogs) hw_draw_logs(4,40,200);
            // Bottom: full options UI (game_render draws it for this mode)
            hw_set_bottom();
            C2D_DrawRectSolid(0,0,0,320,240,C2D_Color32(0,0,0,255));
            game_render();
            hw_end_frame();
            ++frame;
            continue;
        }
    if(gm == GameMode::Editor) {
            // Editor should live on the bottom screen for touch drawing
            // Clear top (optional placeholder/instructions)
            hw_set_top();
            C2D_DrawRectSolid(0,0,0,400,240,C2D_Color32(0,0,0,255));
            hw_draw_text(8,8,"Level Editor (touch to draw)",0xFFFFFFFF);
            if(showTopLogs) hw_draw_logs(4,40,200);
            // Render editor on bottom
            hw_set_bottom();
            C2D_DrawRectSolid(0,0,0,320,240,C2D_Color32(0,0,0,255));
            game_render();
        } else {
            // Normal: gameplay/title etc on top
            hw_set_top();
            game_render();
            if(showTopLogs) hw_draw_logs(4,4,232);
            // Bottom screen overlays (title menu only; gameplay bottom is drawn by game_render)
            hw_set_bottom();
            if(gm == GameMode::Title) {
                // Bottom: simple title UI
                // Screen already cleared at frame-begin; draw buttons + hint
                // Underlay: MENUBOTTOM sheet (single image at index 0) behind the buttons if available
                if (hw_sheet_loaded(HwSheet::MenuBottom)) {
                    C2D_Image under = hw_image_from(HwSheet::MenuBottom, 0);
                    hw_draw_sprite(under, 0, 0, 0, 1.0f, 1.0f);
                }
                hw_draw_text(10, 6, "START=Play  SELECT=Editor  X=Exit", 0xFFFFFFFF);
                game_render_title_buttons(in);
                if(in.touching) {
                    // small crosshair to visualize touch
                    int tx=in.stylusX, ty=in.stylusY;
                    C2D_DrawRectSolid(tx-2, ty, 0, 5,1,C2D_Color32(255,255,0,255));
                    C2D_DrawRectSolid(tx, ty-2, 0, 1,5,C2D_Color32(255,255,0,255));
                }
                if(showBottomLogs) hw_draw_logs(4, 200, 36);
            } else {
                // Gameplay: bottom is rendered by game_render; only optional logs overlay here.
                if(showBottomLogs) hw_draw_logs(2, 220, 18);
            }
        }
        hw_end_frame();
    ++frame;
    }
    sound::shutdown();
    hw_shutdown();
    return 0;
}
