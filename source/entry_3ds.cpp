#include <3ds.h>
#include "hardware.hpp"
#include "IMAGE.h"
#include "IMAGE_t3x.h"
#include "sprite_indexes/image_indices.h"
#include "game.hpp"

int main(int argc, char** argv) {
    if(!hw_init()) return -1;
    game_init();

    while (aptMainLoop()) {
        InputState in; hw_poll_input(in);
        if(in.startPressed) break;
        game_update(in);
        hw_begin_frame();
        game_render();
        hw_end_frame();
    }
    hw_shutdown();
    return 0;
}
