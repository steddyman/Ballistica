#pragma once

namespace options {
// Returns whether music should play (default true). Implemented in options.cpp
bool is_music_enabled();
// Returns whether Emulator Mode is enabled (default false): when true, the two screens are treated as continuous.
bool is_emulator_mode_enabled();
// Load/save persistent options from/to SD card.
void load_settings();
void save_settings();
}
// options.hpp - Options menu module
#pragma once
#include "hardware.hpp"
#include <string>

namespace options {

enum class Action { None, ExitToTitle, SaveAndExit };

// Update options menu when active; returns action for game state machine.
Action update(const InputState &in);
// Render options menu when active.
void render();
// Called when entering options to refresh file list.
void begin();

} // namespace options
