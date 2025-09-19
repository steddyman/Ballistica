// options.cpp - extracted Options menu logic
#include <citro2d.h>
#include <3ds.h>
#include <vector>
#include <string>
#include <cstring>
#include <sys/stat.h>
#include "levels.hpp"
#include "hardware.hpp"
#include "OPTIONS.h"
#include "options.hpp"
#include "ui_button.hpp"
#include "ui_dropdown.hpp"
#include "sound.hpp"

namespace options {

namespace ui {
    constexpr int DD_X=40, DD_Y=60, DD_W=240, DD_H=20;        // dropdown header
    constexpr int NAME_X=40, NAME_Y=100, NAME_W=120, NAME_H=20;
    constexpr int DUP_X=180, DUP_Y=100, DUP_W=120, DUP_H=20;
    constexpr int CANCEL_X=60, CANCEL_Y=190, BTN_W=60, BTN_H=20;
    constexpr int SAVE_X=200, SAVE_Y=190;                     // width/height same as CANCEL
    constexpr int ITEM_H=16;
    constexpr int MAX_VISIBLE=8;
    // Music checkbox placement (label on the left of the box)
    constexpr int MUSIC_LABEL_X = 40;
    constexpr int MUSIC_LABEL_Y = 140;   // label baseline (top of our 6px font)
    constexpr int MUSIC_SZ      = 16;    // checkbox square size
    constexpr int MUSIC_GAP     = 8;     // horizontal gap from label to checkbox
    constexpr int MUSIC_VOFFSET = 0;    // vertical adjust of checkbox relative to label baseline
}

static int selectedIndex = 0; // mirrors dropdown.selectedIndex
static std::string pendingFile; // highlight choice not yet saved
static char duplicateName[9] = {0};
static UIDropdown dropdown; // widget instance
static UIDropdown deviceDD; // device type dropdown
static std::vector<std::string> deviceItems = {"Emulator", "3DS", "3DS XL"};
static std::vector<UIButton> buttons; // NAME, DUPLICATE, CANCEL, SAVE
static bool musicEnabled = true; // default to playing music
// Device selection controls hinge gap. Default: 3DS
static DeviceType currentDevice = DeviceType::ThreeDS;

bool is_music_enabled() { return musicEnabled; }
DeviceType device_type() { return currentDevice; }
int hinge_gap_px() {
    switch (currentDevice) {
        case DeviceType::Emulator: return 0;
        case DeviceType::ThreeDS: return 52;
        case DeviceType::ThreeDSXL: return 68;
    }
    return 52;
}

void load_settings() {
    FILE* f = fopen("sdmc:/ballistica/options.cfg", "rb");
    if (!f) return;
    char key[32]; char val[32];
    while (fscanf(f, "%31[^=]=%31s\n", key, val) == 2) {
        if (strcmp(key, "music") == 0) {
            if (strcmp(val, "0") == 0 || strcasecmp(val, "false") == 0 || strcasecmp(val, "off") == 0) musicEnabled = false;
            else musicEnabled = true;
        } else if (strcmp(key, "device") == 0) {
            if (strcasecmp(val, "emulator") == 0) currentDevice = DeviceType::Emulator;
            else if (strcasecmp(val, "3dsxl") == 0 || strcasecmp(val, "3ds_xl") == 0 || strcasecmp(val, "3ds-xl") == 0) currentDevice = DeviceType::ThreeDSXL;
            else currentDevice = DeviceType::ThreeDS; // default
        }
    }
    fclose(f);
}

void save_settings() {
    // Ensure directory exists; on 3DS fopen won’t create dirs
    mkdir("sdmc:/ballistica", 0777);
    FILE* f = fopen("sdmc:/ballistica/options.cfg", "wb");
    if (!f) return;
    fprintf(f, "music=%s\n", musicEnabled ? "1" : "0");
    const char* devStr = (currentDevice == DeviceType::Emulator) ? "emulator"
                        : (currentDevice == DeviceType::ThreeDSXL) ? "3dsxl"
                        : "3ds";
    fprintf(f, "device=%s\n", devStr);
    fclose(f);
}

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
    // Refresh from disk when entering Options (in case toggled earlier or file changed)
    load_settings();
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
    // Initialize Device Type dropdown to the right of the label
    {
        const char* label = "Device Type:";
        int labelX = ui::MUSIC_LABEL_X;
        int labelY = ui::MUSIC_LABEL_Y + 24;
        int labelW = hw_text_width(label);
        deviceDD = {};
        deviceDD.x = labelX + labelW + 8; // right of label
        deviceDD.y = labelY - 6;          // align header vertically with text
        deviceDD.w = 120;
        deviceDD.h = ui::DD_H;
        deviceDD.itemHeight = ui::ITEM_H;
        deviceDD.maxVisible = 3;
        deviceDD.headerColor = C2D_Color32(40,40,60,255);
        deviceDD.arrowColor  = C2D_Color32(55,55,85,255);
        deviceDD.listBgColor = C2D_Color32(30,30,50,255);
        deviceDD.itemColor   = C2D_Color32(50,50,80,255);
        deviceDD.itemSelColor= C2D_Color32(70,70,110,255);
        ui_dropdown_set_items(deviceDD, deviceItems);
        // Map currentDevice to index
        int idx = 1; // default 3DS
        if (currentDevice == DeviceType::Emulator) idx = 0;
        else if (currentDevice == DeviceType::ThreeDSXL) idx = 2;
        deviceDD.selectedIndex = idx;
        deviceDD.onSelect = [](int sel){
            if (sel <= 0) currentDevice = DeviceType::Emulator;
            else if (sel == 1) currentDevice = DeviceType::ThreeDS;
            else currentDevice = DeviceType::ThreeDSXL;
        };
    }
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
    if (consumed) return Action::None; // level file dropdown handled tap
    ui_dropdown_update(deviceDD, in, consumed);
    if (consumed) return Action::None; // device dropdown handled tap

    if (in.touchPressed) {
        int x=in.stylusX, y=in.stylusY;
        // Handle Music checkbox toggle (label on left)
        {
            const char* label = "Music Enabled";
            int labelW = hw_text_width(label);
            int bx = ui::MUSIC_LABEL_X + labelW + ui::MUSIC_GAP; // checkbox X to the right of label
            int by = ui::MUSIC_LABEL_Y - (ui::MUSIC_SZ - 6)/2 + ui::MUSIC_VOFFSET; // align box center to 6px text height
            int bw = ui::MUSIC_SZ;
            int bh = ui::MUSIC_SZ;
            // Clickable region: from start of label to end of checkbox, union of label height and box height
            int rx0 = ui::MUSIC_LABEL_X;
            int rx1 = bx + bw;
            int ry0 = std::min(ui::MUSIC_LABEL_Y, by);
            int ry1 = std::max(ui::MUSIC_LABEL_Y + 6, by + bh);
            if (x >= rx0 && x < rx1 && y >= ry0 && y < ry1) {
                musicEnabled = !musicEnabled;
                sound::play_sfx("menu-click", 4, 1.0f, true);
                if (!musicEnabled) {
                    sound::stop_music();
                } else {
                    // Resume background track at 80% volume, looped
                    sound::play_music("music", true, 0.8f, true);
                }
                return Action::None;
            }
        }
        // Device Type selection is handled by device dropdown; no tap-to-cycle region needed now.
        for(size_t i=0;i<buttons.size();++i) {
            if (buttons[i].contains(x,y)) {
                // Determine button semantics before trigger (in case of cancel/save we return action)
                // Play click SFX for any options button
                sound::play_sfx("menu-click", 4, 1.0f, true);
                if (buttons[i].label && std::string(buttons[i].label)=="CANCEL") return Action::ExitToTitle;
                if (buttons[i].label && std::string(buttons[i].label)=="SAVE") { apply_save(); save_settings(); return Action::SaveAndExit; }
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
    // Music label (left) and checkbox (right)
    {
        const char* label = "Music Enabled";
        int labelW = hw_text_width(label);
        hw_draw_text(ui::MUSIC_LABEL_X, ui::MUSIC_LABEL_Y, label, 0xFFFFFFFF);
        int bx = ui::MUSIC_LABEL_X + labelW + ui::MUSIC_GAP;
        int by = ui::MUSIC_LABEL_Y - (ui::MUSIC_SZ - 6)/2 + ui::MUSIC_VOFFSET;
        uint32_t boxCol = C2D_Color32(80,80,110,255);
        uint32_t fillCol = C2D_Color32(200,200,255,255);
        // Outline box
        C2D_DrawRectSolid(bx-1, by-1, 0, ui::MUSIC_SZ+2, 1, boxCol); // top
        C2D_DrawRectSolid(bx-1, by+ui::MUSIC_SZ, 0, ui::MUSIC_SZ+2, 1, boxCol); // bottom
        C2D_DrawRectSolid(bx-1, by-1, 0, 1, ui::MUSIC_SZ+2, boxCol); // left
        C2D_DrawRectSolid(bx+ui::MUSIC_SZ, by-1, 0, 1, ui::MUSIC_SZ+2, boxCol); // right
        if (musicEnabled) {
            C2D_DrawRectSolid(bx+3, by+3, 0, ui::MUSIC_SZ-6, ui::MUSIC_SZ-6, fillCol);
        }
    }
    // Device Type selector row: label + dropdown to the right
    {
        const char* label = "Device Type:";
        int labelX = ui::MUSIC_LABEL_X;
        int labelY = ui::MUSIC_LABEL_Y + 24;
        hw_draw_text(labelX, labelY, label, 0xFFFFFFFF);
        // Draw the dropdown header/list
        ui_dropdown_render(deviceDD);
    }
    // Render dropdown last to guarantee its expanded list overlays buttons beneath.
    // Prefer to render the dropdown that is currently open last.
    if (dropdown.open && !deviceDD.open) {
        ui_dropdown_render(dropdown);
    } else if (deviceDD.open && !dropdown.open) {
        ui_dropdown_render(dropdown);
        ui_dropdown_render(deviceDD);
    } else {
        ui_dropdown_render(dropdown);
        ui_dropdown_render(deviceDD);
    }
}

} // namespace options
