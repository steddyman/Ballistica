// ui_button.cpp - drawing helper for UIButton
#include <citro2d.h>
#include "hardware.hpp"
#include "ui_button.hpp"

void ui_draw_button(const UIButton &btn, bool pressed) {
    uint32_t base = btn.color ? btn.color : C2D_Color32(50,50,80,255);
    uint8_t r = base>>24, g=base>>16, b=base>>8, a=base & 0xFF;
    if (pressed) { // darken slightly
        r = (uint8_t)(r*0.7f); g=(uint8_t)(g*0.7f); b=(uint8_t)(b*0.7f);
    }
    C2D_DrawRectSolid(btn.x, btn.y, 0, btn.w, btn.h, C2D_Color32(r,g,b,a));
    if (btn.label)
        hw_draw_text(btn.x + 6, btn.y + (btn.h/2 - 3), btn.label, 0xFFFFFFFF);
    // simple border
    C2D_DrawRectSolid(btn.x, btn.y, 0, btn.w, 1, C2D_Color32(255,255,255,40));
    C2D_DrawRectSolid(btn.x, btn.y+btn.h-1, 0, btn.w, 1, C2D_Color32(0,0,0,120));
    C2D_DrawRectSolid(btn.x, btn.y, 0, 1, btn.h, C2D_Color32(255,255,255,40));
    C2D_DrawRectSolid(btn.x+btn.w-1, btn.y, 0, 1, btn.h, C2D_Color32(0,0,0,120));
}
