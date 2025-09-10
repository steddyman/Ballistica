// options.cpp - extracted Options menu logic
#include <citro2d.h>
#include <3ds.h>
#include <vector>
#include <string>
#include <cstring>
#include "levels.hpp"
#include "hardware.hpp"
#include "OPTIONS.h"
#include "options.hpp"
#include "ui_button.hpp"
#include "ui_dropdown.hpp"

namespace options {

namespace ui {
    constexpr int DD_X=40, DD_Y=60, DD_W=240, DD_H=20;        // dropdown header
    constexpr int NAME_X=40, NAME_Y=100, NAME_W=120, NAME_H=20;
    constexpr int DUP_X=180, DUP_Y=100, DUP_W=120, DUP_H=20;
    constexpr int CANCEL_X=60, CANCEL_Y=190, BTN_W=60, BTN_H=20;
    constexpr int SAVE_X=200, SAVE_Y=190;                     // width/height same as CANCEL
    constexpr int ITEM_H=16;
    constexpr int MAX_VISIBLE=8;
}

static int selectedIndex = 0; // mirrors dropdown.selectedIndex
static std::string pendingFile; // highlight choice not yet saved
static char duplicateName[9] = {0};
static UIDropdown dropdown; // widget instance
static std::vector<UIButton> buttons; // NAME, DUPLICATE, CANCEL, SAVE

static void show_name_keyboard() {
    SwkbdState swkbd; swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, 8);
    swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    swkbdSetInitialText(&swkbd, duplicateName[0] ? duplicateName : "");
    swkbdSetHintText(&swkbd, "Enter name");
    static char out[9]; memset(out,0,sizeof out);
    if (swkbdInputText(&swkbd,out,sizeof out)==SWKBD_BUTTON_CONFIRM) {
        char sanitized[9]; int si=0;
        for(int i=0;i<8 && out[i];++i){ char c=out[i]; if(c>='a'&&c<='z') c = (char)(c-'a'+'A'); if(!((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_')) continue; sanitized[si++]=c; }
        sanitized[si]='\0';
        if(si>0){ strncpy(duplicateName,sanitized,sizeof duplicateName); duplicateName[8]='\0'; }
    }
}

static void duplicate_file(); // fwd

void begin() {
    levels_refresh_files();
    selectedIndex=0; pendingFile.clear();
    auto &files = const_cast<std::vector<std::string>&>(levels_available_files()); // we promise not to modify
    dropdown = {};
    dropdown.x=ui::DD_X; dropdown.y=ui::DD_Y; dropdown.w=ui::DD_W; dropdown.h=ui::DD_H;
    dropdown.itemHeight=ui::ITEM_H; dropdown.maxVisible=ui::MAX_VISIBLE;
    dropdown.headerColor=C2D_Color32(40,40,60,255);
    dropdown.arrowColor=C2D_Color32(55,55,85,255);
    dropdown.listBgColor=C2D_Color32(30,30,50,255);
    dropdown.itemColor=C2D_Color32(50,50,80,255);
    dropdown.itemSelColor=C2D_Color32(70,70,110,255);
    ui_dropdown_set_items(dropdown, files);
    dropdown.onSelect = [](int idx){ selectedIndex = idx; const auto &fl = levels_available_files(); if(idx>=0 && idx < (int)fl.size()) pendingFile = fl[idx]; };
    if(!files.empty()){
        const char *active = levels_get_active_file();
        for(size_t i=0;i<files.size();++i) if(files[i]==active){ dropdown.selectedIndex=(int)i; selectedIndex=(int)i; break; }
        pendingFile = files[dropdown.selectedIndex];
    }
    // Prepare buttons
    buttons.clear();
    UIButton b;
    b={}; b.x=ui::NAME_X; b.y=ui::NAME_Y; b.w=ui::NAME_W; b.h=ui::NAME_H; b.label=""; b.color=C2D_Color32(40,40,60,255); b.onTap=[](){ show_name_keyboard(); }; buttons.push_back(b);
    b={}; b.x=ui::DUP_X; b.y=ui::DUP_Y; b.w=ui::DUP_W; b.h=ui::DUP_H; b.label="DUPLICATE"; b.color=C2D_Color32(60,40,40,255); b.onTap=[](){ duplicate_file(); }; buttons.push_back(b);
    b={}; b.x=ui::CANCEL_X; b.y=ui::CANCEL_Y; b.w=ui::BTN_W; b.h=ui::BTN_H; b.label="CANCEL"; b.color=C2D_Color32(50,30,30,255); buttons.push_back(b);
    b={}; b.x=ui::SAVE_X; b.y=ui::SAVE_Y; b.w=ui::BTN_W; b.h=ui::BTN_H; b.label="SAVE"; b.color=C2D_Color32(30,50,30,255); buttons.push_back(b);
}

static void duplicate_file() {
    if(!duplicateName[0]) return;
    if (levels_duplicate_active(duplicateName)) {
        levels_refresh_files();
        std::string created = std::string(duplicateName)+".DAT";
        const auto &fl = levels_available_files();
        for(size_t i=0;i<fl.size();++i) if(fl[i]==created){ selectedIndex=(int)i; pendingFile=fl[i]; break; }
        // Adjust dropdown scroll if needed
        if (selectedIndex < dropdown.scrollOffset) dropdown.scrollOffset = selectedIndex;
        if (selectedIndex >= dropdown.scrollOffset + dropdown.maxVisible) dropdown.scrollOffset = selectedIndex - dropdown.maxVisible + 1;
        duplicateName[0]='\0';
        hw_log("duplicate ok\n");
    } else hw_log("duplicate failed\n");
}

static void apply_save() {
    if(!pendingFile.empty()) {
        levels_set_active_file(pendingFile.c_str());
        levels_reload_active();
        levels_set_current(0);
        levels_reset_level(0);
    }
}

Action update(const InputState &in) {
    const auto &files = levels_available_files();
    if (selectedIndex < 0 && !files.empty()) selectedIndex = 0;
    if (selectedIndex >= (int)files.size() && !files.empty()) selectedIndex = (int)files.size() - 1;
    if (pendingFile.empty() && !files.empty() && selectedIndex >=0) pendingFile = files[selectedIndex];
    dropdown.selectedIndex = selectedIndex; // sync

    bool consumed=false;
    ui_dropdown_update(dropdown, in, consumed);
    if (consumed) return Action::None; // dropdown handled tap

    if (in.touchPressed) {
        int x=in.stylusX, y=in.stylusY;
        for(size_t i=0;i<buttons.size();++i) {
            if (buttons[i].contains(x,y)) {
                // Determine button semantics before trigger (in case of cancel/save we return action)
                if (buttons[i].label && std::string(buttons[i].label)=="CANCEL") return Action::ExitToTitle;
                if (buttons[i].label && std::string(buttons[i].label)=="SAVE") { apply_save(); return Action::SaveAndExit; }
                buttons[i].trigger();
                return Action::None;
            }
        }
    }
    return Action::None;
}

void render() {
    // Background
    C2D_Image img = hw_image_from(HwSheet::Options, OPTIONS_idx);
    if (img.tex) hw_draw_sprite(img,0,0); else { C2D_DrawRectSolid(0,0,0,320,240,C2D_Color32(20,20,40,255)); hw_draw_text(100,20,"OPTIONS",0xFFFFFFFF); }
    // Name field label & current text (drawn before buttons for consistent layering)
    hw_draw_text(ui::NAME_X, ui::NAME_Y-8, "NAME:", 0xFFFFFFFF);
    // Active file indicator
    const char* active = levels_get_active_file(); if(active && *active) {
        hw_draw_text(ui::DD_X, ui::DD_Y - 12, "Active:", 0xFFFFFFFF);
        hw_draw_text(ui::DD_X + 50, ui::DD_Y - 12, active, 0xFFFFFFFF);
    }
    // Buttons first so dropdown LIST (rendered later) can appear above them when open
    for (const auto &b : buttons) {
        ui_draw_button(b, false);
        if (b.x == ui::NAME_X && b.y == ui::NAME_Y) {
            hw_draw_text(ui::NAME_X+8, ui::NAME_Y+6, duplicateName[0]?duplicateName:"TAP", 0xFFFFFFFF);
        }
    }
    // Render dropdown last to guarantee its expanded list overlays buttons beneath.
    ui_dropdown_render(dropdown);
}

} // namespace options
