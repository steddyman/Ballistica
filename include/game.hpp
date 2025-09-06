// game.hpp - forward declarations for game lifecycle
#pragma once
#include "hardware.hpp"

void game_init();
void game_update(const InputState&);
void game_render();
