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
