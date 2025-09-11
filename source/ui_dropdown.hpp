// ui_dropdown.hpp - reusable dropdown widget
#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include "hardware.hpp"

struct UIDropdown {
    int x=0,y=0,w=0,h=0;              // header rect
    int itemHeight=16;                // per-item height
    int maxVisible=8;                 // visible rows before scroll arrows
    bool open=false;                  // is list expanded
    int selectedIndex=0;              // currently highlighted index (into items)
    int scrollOffset=0;               // top visible index when scrolling
    const std::vector<std::string>* items=nullptr; // external items (must remain valid)
    uint32_t headerColor = 0;         // colors
    uint32_t arrowColor = 0;
    uint32_t listBgColor = 0;
    uint32_t itemColor = 0;
    uint32_t itemSelColor = 0;
    // Callback when selection changes (after user picks item)
    void (*onSelect)(int idx) = nullptr;
};

enum class UIDropdownEvent { None, SelectionChanged };

// Set pointer to items container
inline void ui_dropdown_set_items(UIDropdown &dd, const std::vector<std::string> &items) { dd.items = &items; }

// Handle input (touchPressed edge). Returns event if selection changed. Swallows touch if it interacted.
UIDropdownEvent ui_dropdown_update(UIDropdown &dd, const InputState &in, bool &touchConsumed);

// Draw header + (if open) overlay list.
void ui_dropdown_render(const UIDropdown &dd);
