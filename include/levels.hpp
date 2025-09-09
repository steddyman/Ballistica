// levels.hpp - public level API
#pragma once
#include <cstdint>
#include <vector>
#include <string>

void levels_load();
void levels_render();

int  levels_count();                 // number of loaded levels
int  levels_current();               // current level index
bool levels_set_current(int idx);    // set current, returns success
int  levels_remaining_breakable();
// Lookup atlas index for a raw brick type (returns -1 if invalid)
int levels_atlas_index(int rawType);

// Geometry helpers
int levels_grid_width();
int levels_grid_height();
int levels_left();
int levels_top();
int levels_brick_width();
int levels_brick_height();

// Access brick at grid coordinate; returns raw index (same as BrickType int) or -1
int levels_brick_at(int col, int row);
// Remove (set NB) brick at grid coordinate
void levels_remove_brick(int col, int row);

// Damage a brick at coordinate (for multi-hit like T5). Returns true if brick destroyed this call.
// For non multi-hit bricks behaves like remove & returns true. Returns false if brick still alive.
bool levels_damage_brick(int col,int row);

// Trigger a bomb explosion centered at (col,row). Removes the bomb (if present) and any
// immediate 8-neighbour bricks (including chaining other bombs). Effects are applied by caller.
// Returns number of bricks destroyed (including original) and optionally records destroyed brick
// types into the provided vector if not null.
struct DestroyedBrick { int col; int row; int type; };
int levels_explode_bomb(int col,int row, std::vector<DestroyedBrick>* outDestroyed = nullptr);

// Get remaining HP for a brick (1 for normal breakables, 0 if empty/non-breakable, 1..5 for T5)
int levels_brick_hp(int col,int row);

// Reset (restore) a level's bricks/hp to original snapshot
void levels_reset_level(int index);

// Dynamic level file selection support
const std::vector<std::string>& levels_available_files();
void levels_refresh_files();
void levels_set_active_file(const char* filename); // set desired .DAT (basename)
const char* levels_get_active_file();
void levels_reload_active();

// Duplicate currently selected level file to a new 8-char (max) uppercase name (without extension).
// Returns true on success.
bool levels_duplicate_active(const char* newBaseName);

// -------- Editor support (in-place modification) --------
// Get/set brick in current level (0..levels_count-1) at grid coordinate.
int  levels_edit_get_brick(int levelIndex, int col, int row); // returns -1 on invalid
void levels_edit_set_brick(int levelIndex, int col, int row, int brickType); // silently ignores invalid
int  levels_get_speed(int levelIndex); // 0 if invalid
void levels_set_speed(int levelIndex, int speed); // clamps reasonable range (1..99)
const char* levels_get_name(int levelIndex); // empty string if invalid
void levels_set_name(int levelIndex, const char* name); // truncates to 32 chars
// Save all current in-memory levels back to active .DAT file (overwrite)
bool levels_save_active();
