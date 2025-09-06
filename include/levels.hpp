// levels.hpp - public level API
#pragma once
#include <cstdint>
#include <vector>

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
