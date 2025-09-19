#pragma once

namespace options {
// Returns whether music should play (default true). Implemented in options.cpp
bool is_music_enabled();

// Device selection controls the simulated hinge gap in pixels.
enum class DeviceType { Emulator = 0, ThreeDS = 1, ThreeDSXL = 2 };
// Current device type (default ThreeDS)
DeviceType device_type();
// Returns the hinge gap in pixels derived from the current device type: Emulator=0, 3DS=52, 3DS XL=68
int hinge_gap_px();
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
