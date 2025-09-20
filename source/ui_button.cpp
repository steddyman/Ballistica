// ui_button.cpp - drawing helper for UIButton
#include <citro2d.h>
#include "hardware.hpp"
#include "ui_button.hpp"

void ui_draw_button(const UIButton &btn, bool pressed) {
    uint32_t base = btn.color ? btn.color : C2D_Color32(50,50,80,255);
    uint8_t r = base>>24, g=base>>16, b=base>>8, a=base & 0xFF;
    // Make buttons 20% more opaque by reducing transparency by 20%
    // newAlpha = a + 0.2 * (255 - a)
    {
        int delta = (int)((255 - (int)a) * 0.2f + 0.5f);
        int na = (int)a + delta;
        if (na > 255) na = 255;
        a = (uint8_t)na;
    }
    if (!btn.enabled) {
        // Desaturate and dim when disabled
        uint8_t gray = (uint8_t)((r*30 + g*59 + b*11) / 100);
        r = g = b = gray;
        a = (uint8_t)(a * 0.6f);
    } else if (pressed) { // darken slightly
        r = (uint8_t)(r*0.7f); g=(uint8_t)(g*0.7f); b=(uint8_t)(b*0.7f);
    }
    C2D_DrawRectSolid(btn.x, btn.y, 0, btn.w, btn.h, C2D_Color32(r,g,b,a));
    if (btn.label)
        hw_draw_text(btn.x + 6, btn.y + (btn.h/2 - 2), btn.label, btn.enabled ? 0xFFFFFFFF : C2D_Color32(220,220,220,200)); // lighter when disabled
    // simple border
    uint32_t topL = btn.enabled ? C2D_Color32(255,255,255,40) : C2D_Color32(255,255,255,20);
    uint32_t botR = btn.enabled ? C2D_Color32(0,0,0,120) : C2D_Color32(0,0,0,60);
    C2D_DrawRectSolid(btn.x, btn.y, 0, btn.w, 1, topL);
    C2D_DrawRectSolid(btn.x, btn.y+btn.h-1, 0, btn.w, 1, botR);
    C2D_DrawRectSolid(btn.x, btn.y, 0, 1, btn.h, topL);
    C2D_DrawRectSolid(btn.x+btn.w-1, btn.y, 0, 1, btn.h, botR);
}
