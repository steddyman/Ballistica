// editor.cpp - extracted level editor logic from game.cpp for maintainability
#include <3ds.h>
#include <citro2d.h>
#include <cstring>
#include <string>
#include <cstdio>
#include <sys/stat.h>
#include "hardware.hpp"
#include "levels.hpp"
#include "brick.hpp"
#include "editor.hpp"
#include "DESIGNER.h"
#include "INSTRUCT.h"
#include "ui_button.hpp"
#include "ui_dropdown.hpp"
// IMAGE indices include both e_* (editor) and gameplay sprites
#include "IMAGE.h"
#include "sound.hpp"

namespace editor {

// Geometry & layout (centralized) --------------------------------------------
namespace ui {
    // Four-row layout on bottom screen (240px tall)
    // Row 1: Levels label + level-file dropdown (from Options) — positioned under the grid
    constexpr int Row1Y = 154;
    constexpr int LevelsLabelX = 20, LevelsLabelY = Row1Y + 2;
    constexpr int FileDD_X = LevelsLabelX + 48, FileDD_Y = Row1Y - 2, FileDD_W = 174, FileDD_H = 11;

    // Row 2: Level controls then Speed controls then TEST button
    constexpr int Row2Y = Row1Y + 20;
    constexpr int LabelLevelTextX = 20, LabelLevelTextY = Row2Y + 2;
    // Match previous compact spacing: labelX +40 -> '-', +12 -> value, +16 -> '+'
    constexpr int LevelMinusX = LabelLevelTextX + 49, LevelMinusY = Row2Y + 2; constexpr int LevelPlusX = LabelLevelTextX + 77, LevelPlusY = Row2Y + 2; constexpr int LevelBtnW=10, LevelBtnH=9; // mini buttons
    constexpr int ValueLevelX = LabelLevelTextX + 61, ValueLevelY = Row2Y + 2;

    // Speed group placed after Level group with a small gap
    constexpr int LabelSpeedTextX = LevelPlusX + 22, LabelSpeedTextY = Row2Y + 2;
    constexpr int SpeedMinusX = LabelSpeedTextX + 42, SpeedMinusY = Row2Y + 2; constexpr int SpeedPlusX = LabelSpeedTextX + 71, SpeedPlusY = Row2Y + 2; constexpr int SpeedBtnW=10, SpeedBtnH=9;
    constexpr int ValueSpeedX = LabelSpeedTextX + 55, ValueSpeedY = Row2Y + 2;
    // TEST button follows Speed group with a small gap; nudge ~10px right for alignment
    constexpr int TestBtnX = 208,  TestBtnY = Row2Y - 2, TestBtnW = 34, TestBtnH = 11;

    // Row 3: Commands label + CLEAR, COPY, PASTE, UNDO
    constexpr int Row3Y = Row2Y + 20;
    constexpr int CommandsLabelX = 20, CommandsLabelY = Row3Y + 2;
    constexpr int ClearBtnX= CommandsLabelX + 48, ClearBtnY=Row3Y - 2, ClearBtnW=42, ClearBtnH=11;
    constexpr int CopyBtnY  = Row3Y - 2; constexpr int CopyBtnW=34,  CopyBtnH=11;
    constexpr int PasteBtnY = Row3Y - 2; constexpr int PasteBtnW=40, PasteBtnH=11;
    constexpr int UndoBtnY  = Row3Y - 2; constexpr int UndoBtnW=34,  UndoBtnH=11; // fixed width
    constexpr int CopyPasteGap = 8; // horizontal gap between buttons
    // Fixed X positions for Row 3 buttons (right-aligned UNDO, then PASTE, then COPY; CLEAR anchored left)
    constexpr int UndoBtnX  = 208;
    constexpr int CopyBtnX  = ClearBtnX + ClearBtnW + CopyPasteGap;
    constexpr int PasteBtnX = CopyBtnX + CopyBtnW + CopyPasteGap;

    // Row 4: Status label + saved/dirty indicator + SAVE + EXIT
    constexpr int Row4Y = Row3Y + 20;
    constexpr int StatusLabelX = 20, StatusLabelY = Row4Y + 2;
    constexpr int StatusValueX = 80, StatusValueY = Row4Y + 2;
    constexpr int SaveBtnX=160, SaveBtnY=Row4Y - 2, SaveBtnW=28, SaveBtnH=11;
    constexpr int ExitBtnX=208, ExitBtnY=Row4Y - 2, ExitBtnW=28, ExitBtnH=11;
    // Palette origin
    constexpr int PaletteX=260, PaletteY=52;
    // Labels
    // No Name footer now; labels integrated in rows above
    // Current brick/effect info removed (using palette + INSTRUCT screen instead)
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
    bool dirty = false;        // true if edits not saved
    std::string name;
    bool init = false;
    bool testReturn = false;   // true after hitting TEST until we return
    bool pendingFade = false;  // active during initial Playing fade overlay
    int fadeTimer = 0;         // countdown for fade overlay
    int testGrace = 0;         // frames remaining in grace period after starting a test
    // Drag-paint bookkeeping
    bool wasTouching = false;  // previous frame stylus state
    int lastPaintCol = -1;     // last painted grid column during current drag
    int lastPaintRow = -1;     // last painted grid row during current drag
    bool paintingActive = false; // only paint when press began inside grid
};
static EditorState E;
static std::vector<UIButton> g_buttons; // cached buttons built after init
static EditorAction g_lastAction = EditorAction::None; // set by button lambdas needing a return
static size_t g_pasteIndex = (size_t)-1; // index into g_buttons for disabled state toggling
static size_t g_saveIndex = (size_t)-1;  // index for Save button

// File dropdown
static UIDropdown g_fileDD;
static int g_selectedFileIndex = -1;

// Map BrickType id (0..COUNT-1) to editor atlas index (e_*). Falls back to normal if missing.
static int editor_atlas_index(int brickId) {
    switch (brickId) {
        case (int)BrickType::NB: return IMAGE_e_designer_nobrick_idx; // empty
        case (int)BrickType::YB: return IMAGE_e_yellow_brick_idx;
        case (int)BrickType::GB: return IMAGE_e_green_brick_idx;
        case (int)BrickType::CB: return IMAGE_e_cyan_brick_idx;
        case (int)BrickType::TB: return IMAGE_e_tan_brick_idx;
        case (int)BrickType::PB: return IMAGE_e_purple_brick_idx;
        case (int)BrickType::RB: return IMAGE_e_red_brick_idx;
        case (int)BrickType::LB: return IMAGE_e_life_brick_idx;
        case (int)BrickType::SB: return IMAGE_e_slow_brick_idx;
        case (int)BrickType::FB: return IMAGE_e_fast_brick_idx;
        case (int)BrickType::F1: return IMAGE_e_skull_brick_idx;
        case (int)BrickType::F2: return IMAGE_e_skull_brick_idx;
        case (int)BrickType::B1: return IMAGE_e_b_brick_idx;
        case (int)BrickType::B2: return IMAGE_e_o_brick_idx;
        case (int)BrickType::B3: return IMAGE_e_n_brick_idx;
        case (int)BrickType::B4: return IMAGE_e_u_brick_idx;
        case (int)BrickType::B5: return IMAGE_e_s_brick_idx;
        case (int)BrickType::BS: return IMAGE_e_batsmall_brick_idx;
        case (int)BrickType::BB: return IMAGE_e_batbig_brick_idx;
        case (int)BrickType::ID: return IMAGE_e_indestructible_brick_idx;
        case (int)BrickType::RW: return IMAGE_e_rewind_brick_idx;
        case (int)BrickType::RE: return IMAGE_e_reverse_brick_idx;
        case (int)BrickType::IS: return IMAGE_e_islow_brick_idx;
        case (int)BrickType::IF: return IMAGE_e_ifast_brick_idx;
        case (int)BrickType::AB: return IMAGE_e_another_ball_idx;
        case (int)BrickType::FO: return IMAGE_e_forward_brick_idx;
        case (int)BrickType::LA: return IMAGE_e_laser_brick_idx;
        case (int)BrickType::MB: return IMAGE_e_murderball_brick_idx;
        case (int)BrickType::BA: return IMAGE_e_bonus_brick_idx;
        case (int)BrickType::T5: return IMAGE_e_fivehit_brick_idx;
        case (int)BrickType::BO: return IMAGE_e_bomb_brick_idx;
        case (int)BrickType::OF: return IMAGE_e_offswitch_brick_idx;
        case (int)BrickType::ON: return IMAGE_e_onswitch_brick_idx;
        case (int)BrickType::SS: return IMAGE_e_sideslow_brick_idx;
        case (int)BrickType::SF: return IMAGE_e_sidehard_brick_idx;
        default: return levels_atlas_index(brickId);
    }
}

// ---------------- Undo stack -------------------------------------------------
struct UndoEntry {
    enum class Type { Set, Clear } type;
    int levelIndex = -1;
    // For Set
    int col = -1;
    int row = -1;
    uint8_t prevBrick = 0; // previous brick id
    // For Clear
    std::vector<uint8_t> bricks; // previous full layout (NumBricks entries)
};
static std::vector<UndoEntry> g_undo;
static const size_t kMaxUndo = 128;

static void push_undo_set(int levelIndex, int col, int row, uint8_t prevBrick) {
    UndoEntry ue; ue.type = UndoEntry::Type::Set; ue.levelIndex = levelIndex; ue.col = col; ue.row = row; ue.prevBrick = prevBrick; g_undo.push_back(std::move(ue));
    if (g_undo.size() > kMaxUndo) g_undo.erase(g_undo.begin());
}
static void push_undo_clear(int levelIndex) {
    // Capture full layout
    int gw = levels_grid_width(); int gh = levels_grid_height();
    UndoEntry ue; ue.type = UndoEntry::Type::Clear; ue.levelIndex = levelIndex; ue.bricks.reserve(gw*gh);
    for (int r=0;r<gh;++r) for(int c=0;c<gw;++c) { ue.bricks.push_back((uint8_t)levels_edit_get_brick(levelIndex,c,r)); }
    g_undo.push_back(std::move(ue));
    if (g_undo.size() > kMaxUndo) g_undo.erase(g_undo.begin());
}
static void perform_undo() {
    if (g_undo.empty()) { hw_log("undo: empty\n"); return; }
    UndoEntry ue = g_undo.back();
    g_undo.pop_back();
    if (ue.levelIndex != E.curLevel) { hw_log("undo: level mismatch skipped\n"); return; }
    if (ue.type == UndoEntry::Type::Set) {
        if (ue.col>=0 && ue.row>=0) {
            levels_edit_set_brick(ue.levelIndex, ue.col, ue.row, ue.prevBrick);
        }
    } else if (ue.type == UndoEntry::Type::Clear) {
        int gw = levels_grid_width(); int gh = levels_grid_height();
        int expected = gw*gh;
        if ((int)ue.bricks.size()==expected) {
            int idx=0; for(int r=0;r<gh;++r) for(int c=0;c<gw;++c,++idx) {
                levels_edit_set_brick(ue.levelIndex,c,r, ue.bricks[idx]);
            }
        }
    }
}

// Grid shift helpers (wrap-around)
static void shift_grid_left(int levelIndex) {
    int gw = levels_grid_width();
    int gh = levels_grid_height();
    if (gw <= 0 || gh <= 0) return;
    push_undo_clear(levelIndex);
    std::vector<uint8_t> old(gw*gh, 0);
    for (int r=0;r<gh;++r) for(int c=0;c<gw;++c) {
        int idx=r*gw+c;
        old[idx] = (uint8_t)levels_edit_get_brick(levelIndex,c,r);
    }
    for (int r=0;r<gh;++r) for(int c=0;c<gw;++c) {
        int srcC = (c + 1) % gw; // left shift: new[c] = old[c+1]
        int v = old[r*gw + srcC];
        levels_edit_set_brick(levelIndex, c, r, v);
    }
}
static void shift_grid_right(int levelIndex) {
    int gw = levels_grid_width();
    int gh = levels_grid_height();
    if (gw <= 0 || gh <= 0) return;
    push_undo_clear(levelIndex);
    std::vector<uint8_t> old(gw*gh, 0);
    for (int r=0;r<gh;++r) for(int c=0;c<gw;++c) {
        int idx=r*gw+c;
        old[idx] = (uint8_t)levels_edit_get_brick(levelIndex,c,r);
    }
    for (int r=0;r<gh;++r) for(int c=0;c<gw;++c) {
        int srcC = (c - 1 + gw) % gw; // right shift: new[c] = old[c-1]
        int v = old[r*gw + srcC];
        levels_edit_set_brick(levelIndex, c, r, v);
    }
}
static void shift_grid_up(int levelIndex) {
    int gw = levels_grid_width();
    int gh = levels_grid_height();
    if (gw <= 0 || gh <= 0) return;
    push_undo_clear(levelIndex);
    std::vector<uint8_t> old(gw*gh, 0);
    for (int r=0;r<gh;++r) for(int c=0;c<gw;++c) {
        int idx=r*gw+c;
        old[idx] = (uint8_t)levels_edit_get_brick(levelIndex,c,r);
    }
    for (int r=0;r<gh;++r) for(int c=0;c<gw;++c) {
        int srcR = (r + 1) % gh; // up shift: new[r] = old[r+1]
        int v = old[srcR*gw + c];
        levels_edit_set_brick(levelIndex, c, r, v);
    }
}
static void shift_grid_down(int levelIndex) {
    int gw = levels_grid_width();
    int gh = levels_grid_height();
    if (gw <= 0 || gh <= 0) return;
    push_undo_clear(levelIndex);
    std::vector<uint8_t> old(gw*gh, 0);
    for (int r=0;r<gh;++r) for(int c=0;c<gw;++c) {
        int idx=r*gw+c;
        old[idx] = (uint8_t)levels_edit_get_brick(levelIndex,c,r);
    }
    for (int r=0;r<gh;++r) for(int c=0;c<gw;++c) {
        int srcR = (r - 1 + gh) % gh; // down shift: new[r] = old[r-1]
        int v = old[srcR*gw + c];
        levels_edit_set_brick(levelIndex, c, r, v);
    }
}

// Button auto-size helper: expand width to fit label + padding if needed
static void ui_autosize_button(UIButton &btn, int padding = 12) {
    if (!btn.label) return;
    int tw = hw_text_width(btn.label);
    int target = tw + padding;
    if (target > btn.w) btn.w = target;
}

// No name edit UI in the new layout
// Copy/Paste helpers
static bool editor_copy_exists();
static bool editor_do_copy();
static bool editor_do_paste();
static const char* editor_copy_path() { return "sdmc:/ballistica/editor_copy.bin"; }

// ---------- Copy/Paste implementation --------------------------------------
static bool file_exists(const char* path) {
    struct stat st{}; return stat(path, &st) == 0;
}
static bool editor_copy_exists() {
    return file_exists(editor_copy_path());
}
static bool editor_do_copy() {
    // Write a minimal binary blob with header and bricks only
    const char* path = editor_copy_path();
    FILE* f = fopen(path, "wb");
    if(!f) { hw_log("copy: open fail\n"); return false; }
    uint32_t magic = 0x43505931; // 'CPY1'
    uint32_t w = (uint32_t)levels_grid_width();
    uint32_t h = (uint32_t)levels_grid_height();
    fwrite(&magic,1,4,f); fwrite(&w,1,4,f); fwrite(&h,1,4,f);
    for(int r=0;r<(int)h;++r) for(int c=0;c<(int)w;++c) {
        uint8_t b = (uint8_t)levels_edit_get_brick(E.curLevel,c,r);
        fwrite(&b,1,1,f);
    }
    fclose(f);
    // Ensure Paste button becomes enabled immediately
    if (g_pasteIndex != (size_t)-1 && g_pasteIndex < g_buttons.size()) g_buttons[g_pasteIndex].enabled = true;
    return true;
}
static bool editor_do_paste() {
    const char* path = editor_copy_path();
    FILE* f = fopen(path, "rb");
    if(!f) { hw_log("paste: open fail\n"); return false; }
    uint32_t magic=0,w=0,h=0; if(fread(&magic,1,4,f)!=4||fread(&w,1,4,f)!=4||fread(&h,1,4,f)!=4) { fclose(f); hw_log("paste: header read fail\n"); return false; }
    if(magic != 0x43505931) { fclose(f); hw_log("paste: bad magic\n"); return false; }
    int gw = levels_grid_width(); int gh = levels_grid_height();
    if((int)w != gw || (int)h != gh) { fclose(f); hw_log("paste: size mismatch\n"); return false; }
    // Push undo snapshot of current layout
    push_undo_clear(E.curLevel);
    // Read bricks and apply
    for(int r=0;r<gh;++r) for(int c=0;c<gw;++c) {
        uint8_t b=0; if(fread(&b,1,1,f)!=1) { fclose(f); hw_log("paste: data short\n"); return false; }
        levels_edit_set_brick(E.curLevel,c,r,(int)b);
    }
    fclose(f);
    return true;
}

// Forward helpers -------------------------------------------------------------
static void init_if_needed() {
    if (E.init)
        return;
    levels_load();
    E.curLevel = levels_current();
    E.speed = levels_get_speed(E.curLevel);
    if (E.speed < 10) E.speed = 10; else if (E.speed > 40) E.speed = 40;
    const char *nm = levels_get_name(E.curLevel);
    E.name = nm ? nm : "Level";
    E.init = true;
    // Build UI elements (palette excluded)
    using namespace ui;
    g_buttons.clear();
    UIButton b;
    // TEST button (Row 2)
    b = {}; b.x=TestBtnX; b.y=TestBtnY; b.w=TestBtnW; b.h=TestBtnH; b.label="TEST"; b.color=C2D_Color32(80,80,120,180); ui_autosize_button(b); b.onTap=[](){
        sound::play_sfx("menu-click", 4, 1.0f, true);
        levels_set_current(E.curLevel);
        levels_snapshot_level(E.curLevel); // capture current edits as pristine for test run
        levels_reset_level(E.curLevel);
        E.testReturn = true;
        E.pendingFade = true;
        E.fadeTimer = 90; // ~1.5s
    E.testGrace = 15; // ~0.25s grace (assuming 60fps) preventing immediate auto-return
    hw_log("TEST start\n");
        g_lastAction = EditorAction::StartTest;
    }; g_buttons.push_back(b);
    // CLEAR (Row 3)
    b = {}; b.x=ClearBtnX; b.y=ClearBtnY; b.w=ClearBtnW; b.h=ClearBtnH; b.label="CLEAR"; b.color=C2D_Color32(80,80,120,180); b.onTap=[](){
        sound::play_sfx("menu-click", 4, 1.0f, true);
        push_undo_clear(E.curLevel);
        int gw = levels_grid_width(); int gh = levels_grid_height();
        for (int r = 0; r < gh; ++r) for (int c = 0; c < gw; ++c) levels_edit_set_brick(E.curLevel, c, r, 0);
        E.dirty = true;
    }; g_buttons.push_back(b);
    // UNDO (Row 3)
    b = {}; b.x=UndoBtnX; b.y=UndoBtnY; b.w=UndoBtnW; b.h=UndoBtnH; b.label="UNDO"; b.color=C2D_Color32(80,80,120,180); ui_autosize_button(b); b.onTap=[](){ sound::play_sfx("menu-click", 4, 1.0f, true); perform_undo(); E.dirty = true; }; g_buttons.push_back(b);
    // PASTE (Row 3)
    b = {}; b.w=PasteBtnW; b.h=PasteBtnH; b.label="PASTE"; b.color=C2D_Color32(95,75,135,180);
    b.y = PasteBtnY; b.x = PasteBtnX;
    b.enabled = editor_copy_exists();
    b.onTap=[](){ if(editor_copy_exists()) { if(editor_do_paste()) { sound::play_sfx("menu-click", 4, 1.0f, true); E.dirty = true; } } };
    g_buttons.push_back(b);
    g_pasteIndex = g_buttons.size() - 1;
    // COPY (Row 3)
    b = {}; b.w=CopyBtnW; b.h=CopyBtnH; b.label="COPY"; b.color=C2D_Color32(95,75,135,180);
    b.y = CopyBtnY; b.x = CopyBtnX;
    b.onTap=[](){ if(editor_do_copy()) { sound::play_sfx("menu-click", 4, 1.0f, true); } };
    g_buttons.push_back(b);
    // SAVE (Row 4)
    b = {}; b.x=SaveBtnX; b.y=SaveBtnY; b.w=SaveBtnW; b.h=SaveBtnH; b.label="SAVE"; b.color=C2D_Color32(80,100,140,200); ui_autosize_button(b); b.onTap=[](){ sound::play_sfx("menu-click", 4, 1.0f, true); persist_current_level(); E.dirty=false; }; g_buttons.push_back(b); g_saveIndex = g_buttons.size()-1;
    // EXIT (no save) (Row 4)
    b = {}; b.x=ExitBtnX; b.y=ExitBtnY; b.w=ExitBtnW; b.h=ExitBtnH; b.label="EXIT"; b.color=C2D_Color32(80,80,120,180); ui_autosize_button(b); b.onTap=[](){ sound::play_sfx("menu-click", 4, 1.0f, true); g_lastAction = EditorAction::ExitNoSave; }; g_buttons.push_back(b);

    // Row 3 buttons now use fixed positions; no dynamic relayout needed

    // Initialize file dropdown (Row 1)
    auto &files = const_cast<std::vector<std::string>&>(levels_available_files());
    g_fileDD = {};
    g_fileDD.x=FileDD_X; g_fileDD.y=FileDD_Y; g_fileDD.w=FileDD_W; g_fileDD.h=FileDD_H;
    g_fileDD.itemHeight=14; g_fileDD.maxVisible=6; g_fileDD.headerColor=C2D_Color32(40,40,60,255); g_fileDD.arrowColor=C2D_Color32(55,55,85,255);
    g_fileDD.listBgColor=C2D_Color32(30,30,50,255); g_fileDD.itemColor=C2D_Color32(50,50,80,255); g_fileDD.itemSelColor=C2D_Color32(70,70,110,255);
    ui_dropdown_set_items(g_fileDD, files);
    // Select active; default to 0 if not found
    g_selectedFileIndex = 0;
    g_fileDD.selectedIndex = files.empty() ? -1 : 0;
    const char* active = levels_get_active_file();
    for(size_t i=0;i<files.size();++i){ if(files[i]==active){ g_fileDD.selectedIndex=(int)i; g_selectedFileIndex=(int)i; break; } }
    g_fileDD.onSelect = [](int idx){
        const auto &fl = levels_available_files();
        if(idx>=0 && idx<(int)fl.size()){
            levels_set_active_file(fl[idx].c_str());
            levels_reload_active();
            levels_set_current(0);
            E.curLevel = 0;
            E.speed = levels_get_speed(0);
            if (E.speed < 10) E.speed = 10; else if (E.speed > 40) E.speed = 40;
            const char *nm = levels_get_name(0); E.name = nm?nm:"Level";
            g_undo.clear();
            E.dirty=false;
            // Rebind items and selection in case vector pointer moved during reload
            auto &files2 = const_cast<std::vector<std::string>&>(levels_available_files());
            ui_dropdown_set_items(g_fileDD, files2);
            // Clamp selection to the chosen idx if still valid; otherwise reset to 0
            if (idx >= 0 && idx < (int)files2.size()) {
                g_fileDD.selectedIndex = idx;
            } else if (!files2.empty()) {
                g_fileDD.selectedIndex = 0;
            } else {
                g_fileDD.selectedIndex = -1;
            }
        }
    };
}

void persist_current_level() {
    if (!E.init) return;
    levels_set_speed(E.curLevel, E.speed);
    levels_set_name(E.curLevel, E.name.c_str());
    levels_save_active();
}

void discard_unsaved_changes() {
    // Reload active file from disk to discard any unsaved in-memory edits
    levels_reload_active();
    // Reset current to first level to re-sync state; next init will restore correct current when needed
    levels_set_current(0);
    // Clear editor-local state so UI gets rebuilt on next entry
    g_undo.clear();
    E.dirty = false;
    E.init = false;
}

// (name editing removed)

EditorAction update(const InputState &in) {
    init_if_needed();
    // Keep Paste button enabled state synced to presence of copy file
    if (g_pasteIndex != (size_t)-1 && g_pasteIndex < g_buttons.size()) g_buttons[g_pasteIndex].enabled = editor_copy_exists();
    // Update dropdown interactions first so it can consume touches
    bool consumed=false; ui_dropdown_update(g_fileDD, in, consumed); if(consumed) return EditorAction::None;
    // Support drag-paint across the grid only if the press started inside the grid.
    int x = in.stylusX, y = in.stylusY;
    // Grid region (use editor design cell size)
    int left = levels_edit_left();
    int top = levels_edit_top();
    int cw = levels_edit_brick_width();
    int ch = levels_edit_brick_height();
    int gw = levels_grid_width();
    int gh = levels_grid_height();

    auto within_grid = [&](int px, int py){
        return px >= left && px < left + gw * cw && py >= top && py < top + gh * ch;
    };

    // Start painting only on a fresh press that begins inside the grid
    if (in.touchPressed) {
        if (within_grid(x,y)) {
            E.paintingActive = true;
            E.lastPaintCol = E.lastPaintRow = -1; // reset stroke
        } else {
            E.paintingActive = false; // press outside grid: don't paint
        }
    }

    // Continue painting only while touching and paintingActive
    if (in.touching && E.paintingActive) {
        int col = (x - left) / cw;
        int row = (y - top) / ch;
        if (col >= 0 && col < gw && row >= 0 && row < gh) {
            // Paint only when entering a new cell or the cell differs from current value
            if (col != E.lastPaintCol || row != E.lastPaintRow) {
                int prev = levels_edit_get_brick(E.curLevel, col, row);
                if (prev != E.curBrick) {
                    push_undo_set(E.curLevel, col, row, (uint8_t)prev);
                    levels_edit_set_brick(E.curLevel, col, row, E.curBrick);
                    E.dirty = true;
                }
                E.lastPaintCol = col;
                E.lastPaintRow = row;
            }
        }
        // While painting, don't also trigger button/palette actions
        E.wasTouching = in.touching;
        return EditorAction::None;
    }
    // On release, reset drag tracking so a new stroke can start cleanly and stop painting
    if (E.wasTouching && !in.touching) { E.lastPaintCol = E.lastPaintRow = -1; E.paintingActive = false; }

    // If not painting, only proceed on fresh press for palette/buttons
    if (!in.touchPressed) { E.wasTouching = in.touching; return EditorAction::None; }
    // Arrow hitboxes around grid (mirror render positions)
    {
        int ls = levels_edit_left();
        int ts = levels_edit_top();
        int cw2 = levels_edit_brick_width();
        int ch2 = levels_edit_brick_height();
        int gw2 = levels_grid_width();
        int gh2 = levels_grid_height();
        int gridLeft = ls;
        int gridTop = ts;
        int gridRight = ls + gw2 * cw2;
        int gridBottom = ts + gh2 * ch2;
        int midColX = ls + (gw2 / 2) * cw2 + cw2 / 2;
        int midRowY = ts + (gh2 / 2) * ch2 + ch2 / 2;
        // Left arrow rect
        {
            C2D_Image im = hw_image(IMAGE_e_left_arrow_idx);
            if (im.tex && im.subtex) {
                int w = (int)im.subtex->width;
                int h = (int)im.subtex->height;
                int rx = gridLeft - 1 - w;
                int ry = midRowY - h / 2;
                if (x >= rx && x < rx + w && y >= ry && y < ry + h) { sound::play_sfx("menu-click", 4, 1.0f, true); shift_grid_left(E.curLevel); E.dirty = true; return EditorAction::None; }
            }
        }
        // Right arrow rect
        {
            C2D_Image im = hw_image(IMAGE_e_right_arrow_idx);
            if (im.tex && im.subtex) {
                int w = (int)im.subtex->width;
                int h = (int)im.subtex->height;
                int rx = gridRight + 1;
                int ry = midRowY - h / 2;
                if (x >= rx && x < rx + w && y >= ry && y < ry + h) { sound::play_sfx("menu-click", 4, 1.0f, true); shift_grid_right(E.curLevel); E.dirty = true; return EditorAction::None; }
            }
        }
        // Up arrow rect
        {
            C2D_Image im = hw_image(IMAGE_e_up_arrow_idx);
            if (im.tex && im.subtex) {
                int w = (int)im.subtex->width;
                int h = (int)im.subtex->height;
                int rx = midColX - w / 2;
                int ry = gridTop - 1 - h;
                if (x >= rx && x < rx + w && y >= ry && y < ry + h) { sound::play_sfx("menu-click", 4, 1.0f, true); shift_grid_up(E.curLevel); E.dirty = true; return EditorAction::None; }
            }
        }
        // Down arrow rect
        {
            C2D_Image im = hw_image(IMAGE_e_down_arrow_idx);
            if (im.tex && im.subtex) {
                int w = (int)im.subtex->width;
                int h = (int)im.subtex->height;
                int rx = midColX - w / 2;
                int ry = gridBottom + 1;
                if (x >= rx && x < rx + w && y >= ry && y < ry + h) { sound::play_sfx("menu-click", 4, 1.0f, true); shift_grid_down(E.curLevel); E.dirty = true; return EditorAction::None; }
            }
        }
    }
    // Palette (vertical columns wrapping)
    int palX = ui::PaletteX;
    int palY = ui::PaletteY;
    int pad = 3;
    int itemW = levels_edit_brick_width();
    int itemH = levels_edit_brick_height();
    int bx = palX, by = palY;
    for (int b = 0; b < (int)BrickType::COUNT; ++b) {
        if (by + itemH > 230) { by = palY; bx += itemW + pad; }
    if (x >= bx && x < bx + itemW && y >= by && y < by + itemH) { E.curBrick = b; sound::play_sfx("menu-click", 4, 1.0f, true); return EditorAction::None; }
        by += itemH + pad;
    }
    // Speed - (step 5, clamp to [10,40])
    if (x >= ui::SpeedMinusX && x < ui::SpeedMinusX + ui::SpeedBtnW && y >= ui::SpeedMinusY && y < ui::SpeedMinusY + ui::SpeedBtnH) {
        int s = E.speed - 5; if (s < 10) s = 10; if (s != E.speed) {
            E.speed = s; levels_set_speed(E.curLevel, E.speed); E.dirty = true; sound::play_sfx("menu-click", 4, 1.0f, true);
        }
        return EditorAction::None;
    }
    // Speed + (step 5, clamp to [10,40])
    if (x >= ui::SpeedPlusX && x < ui::SpeedPlusX + ui::SpeedBtnW && y >= ui::SpeedPlusY && y < ui::SpeedPlusY + ui::SpeedBtnH) {
        int s = E.speed + 5; if (s > 40) s = 40; if (s != E.speed) {
            E.speed = s; levels_set_speed(E.curLevel, E.speed); E.dirty = true; sound::play_sfx("menu-click", 4, 1.0f, true);
        }
        return EditorAction::None;
    }
    // Level -
    if (x >= ui::LevelMinusX && x < ui::LevelMinusX + ui::LevelBtnW && y >= ui::LevelMinusY && y < ui::LevelMinusY + ui::LevelBtnH) {
    if (E.curLevel > 0) { E.curLevel--; levels_set_current(E.curLevel); E.speed = levels_get_speed(E.curLevel); if (E.speed < 10) E.speed = 10; else if (E.speed > 40) E.speed = 40; E.name = levels_get_name(E.curLevel); g_undo.clear(); sound::play_sfx("menu-click", 4, 1.0f, true); }
        return EditorAction::None;
    }
    // Level +
    if (x >= ui::LevelPlusX && x < ui::LevelPlusX + ui::LevelBtnW && y >= ui::LevelPlusY && y < ui::LevelPlusY + ui::LevelBtnH) {
    if (E.curLevel + 1 < levels_count()) { E.curLevel++; levels_set_current(E.curLevel); E.speed = levels_get_speed(E.curLevel); if (E.speed < 10) E.speed = 10; else if (E.speed > 40) E.speed = 40; E.name = levels_get_name(E.curLevel); g_undo.clear(); sound::play_sfx("menu-click", 4, 1.0f, true); }
        return EditorAction::None;
    }
    // Dispatch UIButton interactions via onTap
    for (auto &btn : g_buttons) {
        if (btn.contains(x,y)) {
            btn.trigger();
            EditorAction act = g_lastAction;
            g_lastAction = EditorAction::None; // reset
            return act; // may be None
        }
    }
    E.wasTouching = in.touching;
    return EditorAction::None;
}

// Rendering -------------------------------------------------------------------
void render() {
    init_if_needed();
    // Sync Paste button enabled state every frame (in case copy file added/removed externally)
    if (g_pasteIndex != (size_t)-1 && g_pasteIndex < g_buttons.size()) g_buttons[g_pasteIndex].enabled = editor_copy_exists();
    // Background (reuse DESIGNER sheet if present, fallback in caller earlier if needed)
    C2D_Image img = hw_image_from(HwSheet::Designer, DESIGNER_idx);
    if (img.tex) hw_draw_sprite(img, 0, 0);
    else {
        img = hw_image_from(HwSheet::Instruct, INSTRUCT_idx);
        if (img.tex) hw_draw_sprite(img, 0, 0);
        else C2D_DrawRectSolid(0, 0, 0, 320, 240, C2D_Color32(10,10,20,255));
    }
    // Draw level grid using editor (e_*) visuals only (avoid gameplay bricks underneath)
    {
        int gw = levels_grid_width();
        int gh = levels_grid_height();
    int cw = levels_edit_brick_width();
    int ch = levels_edit_brick_height();
    int ls = levels_edit_left();
    int ts = levels_edit_top();
        for (int r = 0; r < gh; ++r) {
            for (int c = 0; c < gw; ++c) {
                int raw = levels_edit_get_brick(E.curLevel, c, r);
                int atlas = editor_atlas_index(raw <= 0 ? (int)BrickType::NB : raw);
                if (atlas < 0) continue;
                hw_draw_sprite(hw_image(atlas), (float)(ls + c * cw), (float)(ts + r * ch));
            }
        }

        // Draw directional arrows around the grid with a 1px gap, aligned to middle row/column
        // Compute grid bounds and midpoints
        float gridLeft = (float)ls;
        float gridTop = (float)ts;
        float gridRight = (float)(ls + gw * cw);
        float gridBottom = (float)(ts + gh * ch);
        float midColX = (float)(ls + (gw / 2) * cw + cw / 2);
        float midRowY = (float)(ts + (gh / 2) * ch + ch / 2);

        // Left arrow: right edge is 1px left of gridLeft
        {
            C2D_Image im = hw_image(IMAGE_e_left_arrow_idx);
            if (im.tex && im.subtex) {
                float w = (float)im.subtex->width;
                float h = (float)im.subtex->height;
                float xf = gridLeft - 1.0f - w;
                float yf = midRowY - h * 0.5f;
                int xi = (int)(xf + 0.5f);
                int yi = (int)(yf + 0.5f);
                hw_draw_sprite(im, (float)xi, (float)yi);
            }
        }
        // Right arrow: left edge is 1px right of gridRight
        {
            C2D_Image im = hw_image(IMAGE_e_right_arrow_idx);
            if (im.tex && im.subtex) {
                float h = (float)im.subtex->height;
                float xf = gridRight + 1.0f;
                float yf = midRowY - h * 0.5f;
                int xi = (int)(xf + 0.5f);
                int yi = (int)(yf + 0.5f);
                hw_draw_sprite(im, (float)xi, (float)yi);
            }
        }
        // Up arrow: bottom edge is 1px above gridTop
        {
            C2D_Image im = hw_image(IMAGE_e_up_arrow_idx);
            if (im.tex && im.subtex) {
                float w = (float)im.subtex->width;
                float h = (float)im.subtex->height;
                float xf = midColX - w * 0.5f;
                float yf = gridTop - 1.0f - h;
                int xi = (int)(xf + 0.5f);
                int yi = (int)(yf + 0.5f);
                hw_draw_sprite(im, (float)xi, (float)yi);
            }
        }
        // Down arrow: top edge is 1px below gridBottom
        {
            C2D_Image im = hw_image(IMAGE_e_down_arrow_idx);
            if (im.tex && im.subtex) {
                float w = (float)im.subtex->width;
                float xf = midColX - w * 0.5f;
                float yf = gridBottom + 1.0f;
                int xi = (int)(xf + 0.5f);
                int yi = (int)(yf + 0.5f);
                hw_draw_sprite(im, (float)xi, (float)yi);
            }
        }
    }
    // Palette
    int cw = levels_edit_brick_width();
    int ch = levels_edit_brick_height();
    int pad = 3;
    int bx = ui::PaletteX, by = ui::PaletteY;
    for (int b = 0; b < (int)BrickType::COUNT; ++b) {
        int atlas = editor_atlas_index(b);
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
    // Row 1: Levels + dropdown (render header later to allow overlay on top of buttons)
    hw_draw_text(ui::LevelsLabelX, ui::LevelsLabelY, "Levels:", 0xFFFFFFFF);

    // UI overlay text for Row 2 (Level/Speed values)
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
    // Row 3: Commands label
    hw_draw_text(ui::CommandsLabelX, ui::CommandsLabelY, "Action:", 0xFFFFFFFF);
    // Draw standard buttons
    for (const auto &b : g_buttons) ui_draw_button(b, false);
    // Minus/plus small buttons (rendered as mini labels)
    label_bg(LevelMinusX, LevelMinusY, "-", true); hw_draw_text(LevelMinusX, LevelMinusY, "-", 0xFFFFFFFF);
    label_bg(LevelPlusX, LevelPlusY, "+", true);  hw_draw_text(LevelPlusX, LevelPlusY, "+", 0xFFFFFFFF);
    label_bg(SpeedMinusX, SpeedMinusY, "-", true); hw_draw_text(SpeedMinusX, SpeedMinusY, "-", 0xFFFFFFFF);
    label_bg(SpeedPlusX, SpeedPlusY, "+", true);  hw_draw_text(SpeedPlusX, SpeedPlusY, "+", 0xFFFFFFFF);
    // Row 4: Status label + indicator
    hw_draw_text(ui::StatusLabelX, ui::StatusLabelY, "Status:", 0xFFFFFFFF);
    // hw_draw_text expects RGBA literals; avoid C2D_Color32 (ABGR) here to prevent channel swap
    uint32_t indCol = E.dirty ? 0xDC3C3CFF /* red */ : 0x3CDC50FF /* green */;
    const char* indTxt = E.dirty? "DIRTY" : "SAVED";
    hw_draw_text(ui::StatusValueX, ui::StatusValueY, indTxt, indCol);
    // No Exit hint

    // Render dropdown last so its overlay can appear above buttons when open
    ui_dropdown_render(g_fileDD);
}

// Fade overlay (rendered by game.cpp while in Playing) ------------------------
bool fade_overlay_active() { return E.testReturn && E.pendingFade; }
void render_fade_overlay() {
    if (!fade_overlay_active()) return;
    C2D_DrawRectSolid(0,0,0,320,240,C2D_Color32(0,0,0,120));
    const char *nm = levels_get_name(levels_current()); if(!nm) nm="Level";
    float scale = 2.0f;
    int tw = hw_text_width(nm);
    int x = (int)((320 - tw * scale) * 0.5f);
    int y = ui::FadeNameY + 24; // shift down 24px
    hw_draw_text_shadow_scaled(x, y, nm, 0xFFFFFFFF, 0x000000FF, scale);
    if (E.fadeTimer>0) E.fadeTimer--; else E.pendingFade=false;
}

// Test return bookkeeping -----------------------------------------------------
bool test_return_active() { return E.testReturn; }
int current_level_index() { return E.curLevel; }
void on_return_from_test() { E.pendingFade=false; E.fadeTimer=0; }
// Updated to fully exit test mode
void on_return_from_test_full() {
    // Restore original brick/hp snapshot so editor shows pre-test state again
    if (E.curLevel >= 0) {
        levels_reset_level(E.curLevel);
    }
    E.pendingFade=false;
    E.fadeTimer=0;
    E.testReturn=false;
    E.testGrace=0;
    hw_log("TEST end\n");
}

bool test_grace_active() { return E.testGrace>0; }
void tick_test_grace() { if(E.testGrace>0) --E.testGrace; }

} // namespace editor
