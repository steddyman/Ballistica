#include <3ds.h>
#include "hardware.hpp"
#include "IMAGE.h"
#include "IMAGE_t3x.h"
#include "sprite_indexes/image_indices.h"
#include "game.hpp"

int main(int argc, char** argv) {
    if(!hw_init()) return -1;
    game_init();
    u32 frame=0;

    while (aptMainLoop()) {
        InputState in; hw_poll_input(in);
        // Exit if game layer requested (X on title or touch EXIT) or START+SELECT chord anywhere as hard quit
        if(exit_requested() || (in.startPressed && in.selectPressed)) break;
        game_update(in);
        hw_begin_frame();
        GameMode gm = game_mode();
        if(gm == GameMode::Editor) {
            // Editor should live on the bottom screen for touch drawing
            // Clear top (optional placeholder/instructions)
            hw_set_top();
            C2D_DrawRectSolid(0,0,0,400,240,C2D_Color32(0,0,0,255));
            hw_draw_text(8,8,"Level Editor (touch to draw)",0xFFFFFFFF);
            // Render editor on bottom
            hw_set_bottom();
            C2D_DrawRectSolid(0,0,0,320,240,C2D_Color32(0,0,0,255));
            game_render();
        } else {
            // Normal: gameplay/title etc on top
            hw_set_top();
            game_render();
            // Bottom screen overlays (title menu or touch-to-move)
            hw_set_bottom();
            C2D_DrawRectSolid(0,0,0,320,240,C2D_Color32(0,0,0,255));
            if(gm == GameMode::Title) {
            // Animated pulse for buttons
            float pulse = 0.5f + 0.5f * std::sin(frame * 0.15f);
            uint8_t accent = (uint8_t)(100 + 80 * pulse);
            struct Btn { int x,y,w,h; const char* label; };
            Btn btns[] = {{60,60,200,20,"PLAY  (START)"},{60,100,200,20,"EDITOR(SELECT)"},{60,140,200,20,"EXIT  (X)"}};
            // Touch handled inside game_update via y-band mapping; here only visual hover highlight
            int tx = in.stylusX, ty = in.stylusY;
            for(auto &bdef : btns) {
                bool over = in.touching && tx>=bdef.x && tx<bdef.x+bdef.w && ty>=bdef.y && ty<bdef.y+bdef.h;
                uint32_t col = over ? C2D_Color32(accent,accent,40,255) : C2D_Color32(40,40,40,255);
                C2D_DrawRectSolid(bdef.x, bdef.y, 0, bdef.w, bdef.h, col);
                // border
                C2D_DrawRectSolid(bdef.x, bdef.y, 0, bdef.w, 1, C2D_Color32(200,200,200,255));
                C2D_DrawRectSolid(bdef.x, bdef.y+bdef.h-1, 0, bdef.w, 1, C2D_Color32(10,10,10,255));
                C2D_DrawRectSolid(bdef.x, bdef.y, 0, 1, bdef.h, C2D_Color32(200,200,200,255));
                C2D_DrawRectSolid(bdef.x+bdef.w-1, bdef.y, 0, 1, bdef.h, C2D_Color32(10,10,10,255));
                hw_draw_text(bdef.x + 20, bdef.y + 6, bdef.label, 0xFFFFFFFF);
            }
            hw_draw_text(20, 20, "START=Play  SELECT=Editor  X=Exit", 0xFFFFFFFF);
            } else {
            // Touch-to-move region (entire bottom unless DEBUG splits)
            bool dbg = false;
#if defined(DEBUG) && DEBUG
            dbg = true;
#endif
            int regionY = dbg ? 120 : 0;
            int regionH = dbg ? 120 : 240;
            // Visual feedback: highlight under stylus & gentle animated background
            float pulse = 0.3f + 0.2f * std::sin(frame * 0.12f);
            uint8_t base = (uint8_t)(20 + 30 * pulse);
            C2D_DrawRectSolid(0,regionY,0,320,regionH,C2D_Color32(base,40,70,255));
            hw_draw_text(70, regionY + regionH/2 - 4, "Touch To Move", 0xFFFFFFFF);
            if(in.touching && in.stylusY>=regionY) {
                int cx = in.stylusX; int cy = in.stylusY;
                int r = 18;
                for(int dy=-r; dy<=r; ++dy) for(int dx=-r; dx<=r; ++dx) if(dx*dx+dy*dy <= r*r) {
                    int px = cx+dx, py = cy+dy; if(px>=0 && px<320 && py>=regionY && py<regionY+regionH)
                        C2D_DrawRectSolid(px,py,0,1,1,C2D_Color32(255,255,255,40));
                }
            }
                if(dbg) { hw_draw_logs(4,4,112); }
            }
        }
        hw_end_frame();
    ++frame;
    }
    hw_shutdown();
    return 0;
}
