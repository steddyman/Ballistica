// editor.cpp - extracted level editor logic from game.cpp for maintainability
#include <3ds.h>
#include <citro2d.h>
#include <cstring>
#include <string>
#include "hardware.hpp"
#include "levels.hpp"
#include "brick.hpp"
#include "editor.hpp"
#include "DESIGNER.h"
#include "INSTRUCT.h"
#include "ui_button.hpp"

namespace editor {

// Geometry & layout (centralized) --------------------------------------------
namespace ui {
    // Button rectangles
    // Button heights increased from 9->11 to add 1px extra padding top & bottom around text
    constexpr int NameBtnX=28,   NameBtnY=7,   NameBtnW=22, NameBtnH=11;
    constexpr int TestBtnX=106,  TestBtnY=220, TestBtnW=40, TestBtnH=11;
    constexpr int ClearBtnX=106, ClearBtnY=177, ClearBtnW=21, ClearBtnH=11;
    constexpr int ExitBtnX=106,   ExitBtnY=193, ExitBtnW=18, ExitBtnH=11;
    constexpr int LevelMinusX=204, LevelMinusY=180; constexpr int LevelPlusX=232, LevelPlusY=180; constexpr int LevelBtnW=10, LevelBtnH=9;
    constexpr int SpeedMinusX=204, SpeedMinusY=196; constexpr int SpeedPlusX=232, SpeedPlusY=196; constexpr int SpeedBtnW=10, SpeedBtnH=9;
    // Palette origin
    constexpr int PaletteX=260, PaletteY=52;
    // Labels
    constexpr int LabelNameX=70, LabelNameY=10;
    constexpr int LabelTestX=TestBtnX, LabelTestY=TestBtnY;
    constexpr int LabelClearX=28, LabelClearY=180;
    constexpr int LabelExitX=28, LabelExitY=196;
    constexpr int LabelLevelMinusX=146, LabelLevelMinusY=LevelMinusY+1;
    constexpr int LabelLevelPlusX=LevelPlusX+1, LabelLevelPlusY=LevelPlusY+1;
    constexpr int LabelSpeedMinusX=146, LabelSpeedMinusY=SpeedMinusY+1;
    constexpr int LabelSpeedPlusX=SpeedPlusX+1, LabelSpeedPlusY=SpeedPlusY+1;
    // HUD text/value positions (previously hard-coded literals in render())
    constexpr int LabelLevelTextX=164, LabelLevelTextY=180;
    constexpr int LabelSpeedTextX=164, LabelSpeedTextY=196;
    constexpr int ValueLevelX=216, ValueLevelY=180;
    constexpr int ValueSpeedX=216, ValueSpeedY=196;
    // Current brick/effect info
    constexpr int CurrentBrickSpriteX=116, CurrentBrickSpriteY=146;
    constexpr int LabelCurrentBrickX=28, LabelCurrentBrickY=148;
    constexpr int LabelEffectX=28, LabelEffectY=164;
    constexpr int ValueEffectX=82, ValueEffectY=164;
    // Exit hint footer
    // constexpr int ExitHintX=10, ExitHintY=230;
    // Fade overlay level name
    constexpr int FadeNameX=100, FadeNameY=110;
}

// Internal editor state -------------------------------------------------------
struct EditorState {
    int curLevel = -1;
    int curBrick = 1;
    int speed = 10;
    std::string name;
    bool init = false;
    bool testReturn = false;   // true after hitting TEST until we return
    bool pendingFade = false;  // active during initial Playing fade overlay
    int fadeTimer = 0;         // countdown for fade overlay
};
static EditorState E;
static std::vector<UIButton> g_buttons; // cached buttons built after init
static EditorAction g_lastAction = EditorAction::None; // set by button lambdas needing a return

// Button auto-size helper: expand width to fit label + padding if needed
static void ui_autosize_button(UIButton &btn, int padding = 12) {
    if (!btn.label) return;
    int tw = hw_text_width(btn.label);
    int target = tw + padding;
    if (target > btn.w) btn.w = target;
}

// Forward decl for name edit (used in button lambda)
static void edit_level_name();

// Forward helpers -------------------------------------------------------------
static void init_if_needed() {
    if (E.init)
        return;
    levels_load();
    E.curLevel = levels_current();
    E.speed = levels_get_speed(E.curLevel);
    if (E.speed <= 0) E.speed = 10;
    const char *nm = levels_get_name(E.curLevel);
    E.name = nm ? nm : "Level";
    E.init = true;
    // Build UI buttons (palette excluded)
    using namespace ui;
    g_buttons.clear();
    UIButton b;
    b = {}; b.x=NameBtnX; b.y=NameBtnY; b.w=NameBtnW; b.h=NameBtnH; b.label="Name"; b.color=C2D_Color32(80,80,120,180); ui_autosize_button(b); b.onTap=[](){ edit_level_name(); }; g_buttons.push_back(b);
    b = {}; b.x=TestBtnX; b.y=TestBtnY; b.w=TestBtnW; b.h=TestBtnH; b.label="Test Level"; b.color=C2D_Color32(80,80,120,180); ui_autosize_button(b); b.onTap=[](){
        levels_set_current(E.curLevel);
        levels_snapshot_level(E.curLevel); // capture current edits as pristine for test run
        levels_reset_level(E.curLevel);
        E.testReturn = true;
        E.pendingFade = true;
        E.fadeTimer = 90; // ~1.5s
        g_lastAction = EditorAction::StartTest;
    }; g_buttons.push_back(b);
    b = {}; b.x=ClearBtnX; b.y=ClearBtnY; b.w=ClearBtnW; b.h=ClearBtnH; b.label="Clear"; b.color=C2D_Color32(80,80,120,180); ui_autosize_button(b); b.onTap=[](){
        int gw = levels_grid_width(); int gh = levels_grid_height();
        for (int r = 0; r < gh; ++r) for (int c = 0; c < gw; ++c) levels_edit_set_brick(E.curLevel, c, r, 0);
    }; g_buttons.push_back(b);
    b = {}; b.x=ExitBtnX; b.y=ExitBtnY; b.w=ExitBtnW; b.h=ExitBtnH; b.label="Exit"; b.color=C2D_Color32(80,80,120,180); ui_autosize_button(b); b.onTap=[](){
        persist_current_level();
        g_lastAction = EditorAction::SaveAndExit;
    }; g_buttons.push_back(b);
}

void persist_current_level() {
    if (!E.init) return;
    levels_set_speed(E.curLevel, E.speed);
    levels_set_name(E.curLevel, E.name.c_str());
    levels_save_active();
}

// Software keyboard for renaming current level (16 chars)
static void edit_level_name() {
    SwkbdState sw;
    swkbdInit(&sw, SWKBD_TYPE_NORMAL, 2, 16);
    swkbdSetValidation(&sw, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    swkbdSetInitialText(&sw, E.name.c_str());
    static char out[33];
    memset(out, 0, sizeof out);
    if (swkbdInputText(&sw, out, sizeof out) == SWKBD_BUTTON_CONFIRM) {
        E.name = out;
        levels_set_name(E.curLevel, E.name.c_str());
    }
}

EditorAction update(const InputState &in) {
    init_if_needed();
    if (!in.touchPressed)
        return EditorAction::None;

    int x = in.stylusX, y = in.stylusY;
    // Grid region
    int left = levels_left();
    int top = levels_top();
    int cw = levels_brick_width();
    int ch = levels_brick_height();
    int gw = levels_grid_width();
    int gh = levels_grid_height();
    if (x >= left && x < left + gw * cw && y >= top && y < top + gh * ch) {
        int col = (x - left) / cw;
        int row = (y - top) / ch;
        levels_edit_set_brick(E.curLevel, col, row, E.curBrick);
        return EditorAction::None;
    }
    // Palette (vertical columns wrapping)
    int palX = ui::PaletteX;
    int palY = ui::PaletteY;
    int pad = 3;
    int itemW = cw;
    int itemH = ch;
    int bx = palX, by = palY;
    for (int b = 0; b < (int)BrickType::COUNT; ++b) {
        if (by + itemH > 230) { by = palY; bx += itemW + pad; }
        if (x >= bx && x < bx + itemW && y >= by && y < by + itemH) { E.curBrick = b; return EditorAction::None; }
        by += itemH + pad;
    }
    // Speed -
    if (x >= ui::SpeedMinusX && x < ui::SpeedMinusX + ui::SpeedBtnW && y >= ui::SpeedMinusY && y < ui::SpeedMinusY + ui::SpeedBtnH) {
        if (E.speed > 1) { E.speed--; levels_set_speed(E.curLevel, E.speed); }
        return EditorAction::None;
    }
    // Speed +
    if (x >= ui::SpeedPlusX && x < ui::SpeedPlusX + ui::SpeedBtnW && y >= ui::SpeedPlusY && y < ui::SpeedPlusY + ui::SpeedBtnH) {
        if (E.speed < 99) { E.speed++; levels_set_speed(E.curLevel, E.speed); }
        return EditorAction::None;
    }
    // Level -
    if (x >= ui::LevelMinusX && x < ui::LevelMinusX + ui::LevelBtnW && y >= ui::LevelMinusY && y < ui::LevelMinusY + ui::LevelBtnH) {
        if (E.curLevel > 0) { E.curLevel--; levels_set_current(E.curLevel); E.speed = levels_get_speed(E.curLevel); E.name = levels_get_name(E.curLevel); }
        return EditorAction::None;
    }
    // Level +
    if (x >= ui::LevelPlusX && x < ui::LevelPlusX + ui::LevelBtnW && y >= ui::LevelPlusY && y < ui::LevelPlusY + ui::LevelBtnH) {
        if (E.curLevel + 1 < levels_count()) { E.curLevel++; levels_set_current(E.curLevel); E.speed = levels_get_speed(E.curLevel); E.name = levels_get_name(E.curLevel); }
        return EditorAction::None;
    }
    // Dispatch UIButton interactions (Name, Test, Clear, Exit) via onTap
    for (auto &btn : g_buttons) {
        if (btn.contains(x,y)) {
            btn.trigger();
            EditorAction act = g_lastAction;
            g_lastAction = EditorAction::None; // reset
            return act; // may be None
        }
    }
    return EditorAction::None;
}

// Rendering -------------------------------------------------------------------
void render() {
    init_if_needed();
    // Background (reuse DESIGNER sheet if present, fallback in caller earlier if needed)
    C2D_Image img = hw_image_from(HwSheet::Designer, DESIGNER_idx);
    if (img.tex) hw_draw_sprite(img, 0, 0);
    else {
        img = hw_image_from(HwSheet::Instruct, INSTRUCT_idx);
        if (img.tex) hw_draw_sprite(img, 0, 0);
        else C2D_DrawRectSolid(0, 0, 0, 320, 240, C2D_Color32(10,10,20,255));
    }
    levels_render();
    // Palette
    int cw = levels_brick_width();
    int ch = levels_brick_height();
    int pad = 3;
    int bx = ui::PaletteX, by = ui::PaletteY;
    for (int b = 0; b < (int)BrickType::COUNT; ++b) {
        int atlas = levels_atlas_index(b);
        if (atlas >= 0) hw_draw_sprite(hw_image(atlas), bx, by);
        if (b == E.curBrick) {
            C2D_DrawRectSolid(bx - 1, by - 1, 0, cw + 2, 1, C2D_Color32(255,255,255,255));
            C2D_DrawRectSolid(bx - 1, by + ch, 0, cw + 2, 1, C2D_Color32(255,255,255,255));
            C2D_DrawRectSolid(bx - 1, by, 0, 1, ch, C2D_Color32(255,255,255,255));
            C2D_DrawRectSolid(bx + cw, by, 0, 1, ch, C2D_Color32(255,255,255,255));
        }
        if (by + ch > 230 - ch) { by = ui::PaletteY; bx += cw + pad; }
        else by += ch + pad;
    }
    // UI overlay text
    char buf[32];
    hw_draw_text(ui::LabelLevelTextX, ui::LabelLevelTextY, "Level:", 0xFFFFFFFF);
    hw_draw_text(ui::LabelSpeedTextX, ui::LabelSpeedTextY, "Speed:", 0xFFFFFFFF);
    snprintf(buf, sizeof buf, "%02d", E.curLevel + 1); hw_draw_text(ui::ValueLevelX, ui::ValueLevelY, buf, 0xFFFFFFFF);
    snprintf(buf, sizeof buf, "%02d", E.speed);       hw_draw_text(ui::ValueSpeedX, ui::ValueSpeedY, buf, 0xFFFFFFFF);

    auto label_bg = [](int textX, int textY, const char *txt, bool small=false) {
        if (!txt)
            return;
        int len = (int)std::strlen(txt);
        if (!len)
            return;
        int charW = 6; // font width
        int padX = 2;
        int w = len * charW + padX * 2;
        int h = 8;
        int boxX = textX - padX;
        int boxY = textY - 2;
        uint32_t col = small ? C2D_Color32(70,70,110,180) : C2D_Color32(80,80,120,180);
        C2D_DrawRectSolid(boxX, boxY, 0, w, h, col);
    };
    using namespace ui;
    // Draw standard buttons
    for (const auto &b : g_buttons) ui_draw_button(b, false);
    // Minus/plus small buttons (rendered as mini labels)
    label_bg(LevelMinusX, LevelMinusY, "-", true); hw_draw_text(LevelMinusX, LevelMinusY, "-", 0xFFFFFFFF);
    label_bg(LevelPlusX, LevelPlusY, "+", true);  hw_draw_text(LevelPlusX, LevelPlusY, "+", 0xFFFFFFFF);
    label_bg(SpeedMinusX, SpeedMinusY, "-", true); hw_draw_text(SpeedMinusX, SpeedMinusY, "-", 0xFFFFFFFF);
    label_bg(SpeedPlusX, SpeedPlusY, "+", true);  hw_draw_text(SpeedPlusX, SpeedPlusY, "+", 0xFFFFFFFF);
    int atlas = levels_atlas_index(E.curBrick);
    if (atlas >= 0) hw_draw_sprite(hw_image(atlas), ui::CurrentBrickSpriteX, ui::CurrentBrickSpriteY);
    hw_draw_text(ui::LabelCurrentBrickX, ui::LabelCurrentBrickY, "Current Brick:", 0xFFFFFFFF);
    hw_draw_text(ui::LabelEffectX, ui::LabelEffectY, "Effect:", 0xFFFFFFFF);
    hw_draw_text(ui::LabelClearX, ui::LabelClearY, "Clear Level:", 0xFFFFFFFF);
    hw_draw_text(ui::LabelExitX, ui::LabelExitY, "Save / Exit:", 0xFFFFFFFF);
    static const char *effectNames[] = {"Empty","10 Points","20 Points","30 Points","40 Points","50 Points","100 Points","Extra Life","Slow Ball","Fast Ball","Skull Slow","Skull Fast","Bonus B","Bonus O","Bonus N","Bonus U","Bonus S","Bat Small","Bat Big","Indestructible Brick","Level Rewind","Reverse Controls","Slow Now","Fast Now","Add Ball","Level Forward","Laser","MurderBall","Bonus","Five Hits","Bomb","Lights Off","Lights On","Moving Brick","Hard Moving Brick"};
    if (E.curBrick >=0 && E.curBrick < (int)(sizeof(effectNames)/sizeof(effectNames[0]))) hw_draw_text(ui::ValueEffectX, ui::ValueEffectY, effectNames[E.curBrick], 0xFFFFFFFF);
    // hw_draw_text(LabelNameX, LabelNameY, "Name", 0xFFFFFFFF);
    hw_draw_text(LabelNameX, LabelNameY, E.name.c_str(), 0xFFFFFFFF);
    // hw_draw_text(ui::ExitHintX, ui::ExitHintY, "Tap Exit to Save", 0xFFFFFFFF);
}

// Fade overlay (rendered by game.cpp while in Playing) ------------------------
bool fade_overlay_active() { return E.testReturn && E.pendingFade; }
void render_fade_overlay() {
    if (!fade_overlay_active()) return;
    C2D_DrawRectSolid(0,0,0,320,240,C2D_Color32(0,0,0,120));
    const char *nm = levels_get_name(levels_current()); if(!nm) nm="Level"; hw_draw_text(ui::FadeNameX, ui::FadeNameY, nm, 0xFFFFFFFF);
    if (E.fadeTimer>0) E.fadeTimer--; else E.pendingFade=false;
}

// Test return bookkeeping -----------------------------------------------------
bool test_return_active() { return E.testReturn; }
int current_level_index() { return E.curLevel; }
void on_return_from_test() { E.pendingFade=false; E.fadeTimer=0; }

} // namespace editor
