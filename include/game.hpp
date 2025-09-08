// game.hpp - forward declarations for game lifecycle
#pragma once
#include "hardware.hpp"

// game.hpp - forward declarations for game lifecycle
#pragma once
#include "hardware.hpp"
void game_init();
void game_update(const InputState&);
void game_render();

enum class GameMode { Title, Playing, Editor };
GameMode game_mode();

// Level helpers (temporary minimal port)
void levels_load();
void levels_render();
bool exit_requested();
