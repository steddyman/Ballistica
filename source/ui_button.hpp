// ui_button.hpp - simple reusable button primitive for 3DS touch UI
#pragma once
#include <functional>
#include <cstdint>
#include <string>

struct UIButton {
    int x=0,y=0,w=0,h=0;          // rectangle in bottom screen coordinates
    const char* label=nullptr;    // optional static label (can be nullptr if custom draw)
    uint32_t color = 0;           // base fill color (RGBA)
    std::function<void()> onTap;  // invoked on touchPressed inside rect
    bool highlighted=false;       // could be used for hover/focus states (future)
    bool enabled=true;            // disabled buttons draw dim and ignore taps

    bool contains(int px,int py) const { return px>=x && px<x+w && py>=y && py<y+h; }
    void trigger() { if(enabled && onTap) onTap(); }
};

// Utility draw routine (implemented in ui_button.cpp)
void ui_draw_button(const UIButton &btn, bool pressed=false);
