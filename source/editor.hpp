// editor.hpp - extracted level editor logic
#pragma once
#include "hardware.hpp" // InputState
#include <cstdint>

namespace editor {

// Actions the editor requests the game state machine perform this frame.
enum class EditorAction {
    None,
    StartTest,      // switch to Playing mode (test run)
    SaveAndExit     // save + switch to Title
};

// Advance editor logic for one frame when in Editor game mode.
// Returns an action the caller (game.cpp) must perform.
EditorAction update(const InputState &in);

// Render the editor (only call when game mode is Editor).
void render();

// Fade overlay (shown after launching test run) --------------------------------
// True while the translucent name overlay should still be drawn in Playing mode.
bool fade_overlay_active();
// Draw and tick fade overlay (call only if fade_overlay_active()).
void render_fade_overlay();

// Test-run return handling -----------------------------------------------------
// True if current Playing session originated from Editor "TEST" action and
// should return to Editor after completion / life loss.
bool test_return_active();
// Level index that Editor was on when test started.
int current_level_index();
// Called when game transitions back from test Playing run into Editor.
void on_return_from_test();

// Update persistent metadata (speed/name) prior to exit (already done internally
// for SaveAndExit action, but exposed for any future external triggers).
void persist_current_level();

} // namespace editor
