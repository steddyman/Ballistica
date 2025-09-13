namespace game {
	void spawn_dust_effect(float x, float y);
}
// game.hpp - forward declarations for game lifecycle
#pragma once
#include "hardware.hpp"

// game.hpp - forward declarations for game lifecycle
#pragma once
#include "hardware.hpp"
void game_init();
void game_update(const InputState&);
void game_render();
// Renders title screen interactive buttons (bottom screen). Pass current input for hover highlight.
void game_render_title_buttons(const InputState&);

enum class GameMode { Title, Playing, Editor, Options };
GameMode game_mode();

// Level helpers (temporary minimal port)
void levels_load();
void levels_render();
bool exit_requested();
