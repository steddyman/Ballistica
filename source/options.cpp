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

static int selectedIndex = 0;
static bool dropdownOpen = false;
static int scrollOffset = 0;
static std::string pendingFile; // highlight choice not yet saved
static char duplicateName[9] = {0};

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

void begin() {
    levels_refresh_files();
    dropdownOpen=false; scrollOffset=0; selectedIndex=0; pendingFile.clear();
    const auto &files = levels_available_files();
    if(!files.empty()){
        const char *active = levels_get_active_file();
        for(size_t i=0;i<files.size();++i) if(files[i]==active){ selectedIndex=(int)i; break; }
        pendingFile = files[selectedIndex];
    }
}

static void duplicate_file() {
    if(!duplicateName[0]) return;
    if (levels_duplicate_active(duplicateName)) {
        levels_refresh_files();
        std::string created = std::string(duplicateName)+".DAT";
        const auto &fl = levels_available_files();
        for(size_t i=0;i<fl.size();++i) if(fl[i]==created){ selectedIndex=(int)i; pendingFile=fl[i]; break; }
        if (selectedIndex < scrollOffset) scrollOffset = selectedIndex;
        if (selectedIndex >= scrollOffset + ui::MAX_VISIBLE) scrollOffset = selectedIndex - ui::MAX_VISIBLE + 1;
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

    if (in.touchPressed) {
        int x=in.stylusX, y=in.stylusY;
        using namespace ui;
        if (dropdownOpen) {
            bool scrolling = files.size() > (size_t)MAX_VISIBLE;
            int listStartY = DD_Y + DD_H; // below header
            int overlayH = scrolling ? (MAX_VISIBLE + 2) * ITEM_H : (int)files.size() * ITEM_H;
            int overlayY1 = listStartY + overlayH;
            if (x >= DD_X && x < DD_X+DD_W && y >= listStartY && y < overlayY1) {
                if (scrolling) {
                    int topArrowY = listStartY;
                    int itemsY0 = topArrowY + ITEM_H;
                    int itemsY1 = itemsY0 + MAX_VISIBLE * ITEM_H;
                    int bottomArrowY0 = itemsY1;
                    if (y >= topArrowY && y < topArrowY + ITEM_H) { if (scrollOffset>0) scrollOffset--; return Action::None; }
                    if (y >= bottomArrowY0 && y < bottomArrowY0 + ITEM_H) { if (scrollOffset + MAX_VISIBLE < (int)files.size()) scrollOffset++; return Action::None; }
                    if (y >= itemsY0 && y < itemsY1) {
                        int rel = (y - itemsY0) / ITEM_H; int idx = scrollOffset + rel;
                        if (idx>=0 && idx < (int)files.size()) { selectedIndex = idx; pendingFile = files[idx]; dropdownOpen=false; }
                        return Action::None;
                    }
                    dropdownOpen=false; return Action::None; // inside overlay but not on items
                } else { // non scrolling
                    int rel = (y - listStartY) / ITEM_H;
                    int idx = rel;
                    if (idx>=0 && idx < (int)files.size()) {
                        selectedIndex=idx;
                        pendingFile=files[idx];
                        dropdownOpen=false;
                    } else {
                        dropdownOpen=false;
                    }
                    return Action::None;
                }
            }
            // tap elsewhere closes
            if (x >= DD_X && x < DD_X+DD_W && y >= DD_Y && y < DD_Y+DD_H) { dropdownOpen=false; return Action::None; }
            dropdownOpen=false; return Action::None; // swallow
        }
        // Name input
        if (x>=NAME_X && x<NAME_X+NAME_W && y>=NAME_Y && y<NAME_Y+NAME_H) { show_name_keyboard(); return Action::None; }
        // Duplicate
        if (x>=DUP_X && x<DUP_X+DUP_W && y>=DUP_Y && y<DUP_Y+DUP_H) { duplicate_file(); return Action::None; }
        // Cancel
        if (x>=CANCEL_X && x<CANCEL_X+BTN_W && y>=CANCEL_Y && y<CANCEL_Y+BTN_H) { return Action::ExitToTitle; }
        // Save
        if (x>=SAVE_X && x<SAVE_X+BTN_W && y>=SAVE_Y && y<SAVE_Y+BTN_H) { apply_save(); return Action::SaveAndExit; }
        // Dropdown header
        if (x>=DD_X && x<DD_X+DD_W && y>=DD_Y && y<DD_Y+DD_H) {
            if(!dropdownOpen) {
                dropdownOpen=true;
                if (selectedIndex < scrollOffset) scrollOffset = selectedIndex;
                if (selectedIndex >= scrollOffset + MAX_VISIBLE) scrollOffset = selectedIndex - MAX_VISIBLE + 1;
            }
            return Action::None;
        }
        if (dropdownOpen) dropdownOpen=false; // generic close
    }
    return Action::None;
}

void render() {
    // Background
    C2D_Image img = hw_image_from(HwSheet::Options, OPTIONS_idx);
    if (img.tex) hw_draw_sprite(img,0,0); else { C2D_DrawRectSolid(0,0,0,320,240,C2D_Color32(20,20,40,255)); hw_draw_text(100,20,"OPTIONS",0xFFFFFFFF); }
    using namespace ui;
    const auto &files = levels_available_files();
    // Header box
    C2D_DrawRectSolid(DD_X,DD_Y,0,DD_W,DD_H,C2D_Color32(40,40,60,255));
    hw_draw_text(DD_X+8,DD_Y+6, files.empty()?"(no .DAT)":(files.size()>(size_t)selectedIndex?files[selectedIndex].c_str():"?"), 0xFFFFFFFF);
    // Arrow box
    int arrowBoxX = DD_X + DD_W - 16; int arrowBoxW=16; int arrowBoxH=DD_H;
    C2D_DrawRectSolid(arrowBoxX,DD_Y,0,arrowBoxW,arrowBoxH,C2D_Color32(55,55,85,255));
    int triH=7; int triW = 1+(triH-1)*2; if(triW>11) triW=11; int triCx=arrowBoxX+arrowBoxW/2; int midY=DD_Y+arrowBoxH/2;
    if (dropdownOpen) { int apexY=midY-triH/2; for(int row=0;row<triH;++row){int span=1+row*2; if(span>triW) span=triW; int x=triCx-span/2; int y=apexY+row; C2D_DrawRectSolid(x,y,0,span,1,C2D_Color32(200,200,230,255)); } }
    else { int apexY=midY+triH/2; for(int row=0;row<triH;++row){int span=1+row*2; if(span>triW) span=triW; int x=triCx-span/2; int y=apexY-row; C2D_DrawRectSolid(x,y,0,span,1,C2D_Color32(200,200,230,255)); } }
    // Name input
    hw_draw_text(NAME_X, NAME_Y-8, "NAME:", 0xFFFFFFFF);
    C2D_DrawRectSolid(NAME_X,NAME_Y,0,NAME_W,NAME_H,C2D_Color32(40,40,60,255));
    hw_draw_text(NAME_X+8, NAME_Y+6, duplicateName[0]?duplicateName:"TAP", 0xFFFFFFFF);
    // Duplicate button
    C2D_DrawRectSolid(DUP_X,DUP_Y,0,DUP_W,DUP_H,C2D_Color32(60,40,40,255));
    hw_draw_text(DUP_X+8,DUP_Y+6,"DUPLICATE",0xFFFFFFFF);
    // Cancel
    C2D_DrawRectSolid(CANCEL_X,CANCEL_Y,0,BTN_W,BTN_H,C2D_Color32(50,30,30,255)); hw_draw_text(CANCEL_X+10,CANCEL_Y+8,"CANCEL",0xFFFFFFFF);
    // Save
    C2D_DrawRectSolid(SAVE_X,SAVE_Y,0,BTN_W,BTN_H,C2D_Color32(30,50,30,255)); hw_draw_text(SAVE_X+16,SAVE_Y+8,"SAVE",0xFFFFFFFF);
    // Dropdown list overlay
    if (dropdownOpen) {
        if (files.size() > (size_t)ui::MAX_VISIBLE) {
            if (scrollOffset < 0) {
                scrollOffset=0;
            }
            int maxStart = (int)files.size()-ui::MAX_VISIBLE;
            if (maxStart<0) {
                maxStart=0;
            }
            if (scrollOffset>maxStart) {
                scrollOffset=maxStart;
            }
            int h=(ui::MAX_VISIBLE + 2)*ITEM_H; int boxY = DD_Y + DD_H; C2D_DrawRectSolid(DD_X,boxY,0,DD_W,h,C2D_Color32(30,30,50,255));
            // Top arrow area
            C2D_DrawRectSolid(DD_X+2, boxY+2,0,DD_W-4, ITEM_H-4, C2D_Color32(50,50,80,255)); hw_draw_text(DD_X+DD_W/2-12, boxY+4, "UP",0xFFFFFFFF);
            int itemsY0 = boxY + ITEM_H;
            for(int vis=0; vis<ui::MAX_VISIBLE; ++vis){ int fi = scrollOffset + vis; if (fi >= (int)files.size()) break; int iy = itemsY0 + vis*ITEM_H; uint32_t col = (fi==selectedIndex)?C2D_Color32(70,70,110,255):C2D_Color32(50,50,80,255); C2D_DrawRectSolid(DD_X+2, iy+1,0,DD_W-4, ITEM_H-2, col); hw_draw_text(DD_X+6, iy+4, files[fi].c_str(), 0xFFFFFFFF);}            
            int bottomY = itemsY0 + ui::MAX_VISIBLE * ITEM_H; C2D_DrawRectSolid(DD_X+2, bottomY+2,0,DD_W-4, ITEM_H-4, C2D_Color32(50,50,80,255)); hw_draw_text(DD_X+DD_W/2-16, bottomY+4, "DOWN",0xFFFFFFFF);
        } else {
            int h = (int)files.size()*ITEM_H; int boxY = DD_Y + DD_H; C2D_DrawRectSolid(DD_X, boxY,0,DD_W,h, C2D_Color32(30,30,50,255));
            for(size_t i=0;i<files.size();++i){ int iy = boxY + (int)i*ITEM_H; uint32_t col=(i==(size_t)selectedIndex)?C2D_Color32(70,70,110,255):C2D_Color32(50,50,80,255); C2D_DrawRectSolid(DD_X+2, iy+1,0,DD_W-4, ITEM_H-2, col); hw_draw_text(DD_X+6, iy+4, files[i].c_str(),0xFFFFFFFF);}        
        }
    }
}

} // namespace options
