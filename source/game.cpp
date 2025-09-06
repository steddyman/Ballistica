#include <vector>
#include <algorithm>
#include <cstdint>
#include "hardware.hpp"
#include "IMAGE.h"
#include "IMAGE_t3x.h"
#include "sprite_indexes/image_indices.h"

// Basic game state migrated from legacy structures (incremental port)
namespace game {
    struct Ball { float x, y; float vx, vy; bool active; C2D_Image img; };
    struct Bat  { float x, y; float width, height; C2D_Image img; };
    struct State {
        Bat bat{};
        std::vector<Ball> balls;
        C2D_Image imgBall{};
        C2D_Image imgBatNormal{};
    };

    static State G;

    void init_assets() {
        G.imgBall = hw_image(IMAGE_ball_sprite_idx);
        G.imgBatNormal = hw_image(IMAGE_bat_normal_idx);
        G.bat = { 160.f - 32.f, 200.f, 64.f, 8.f, G.imgBatNormal };
        G.balls.push_back({160.f - 4.f, 180.f, 0.0f, -1.5f, true, G.imgBall});
    }

    void init() {
    hw_log("game_init\n");
        init_assets();
    hw_log("assets loaded\n");
    }

    void update(const InputState& in) {
    hw_log("u"); // very lightweight heartbeat (one char per frame)
        // Stylus controls bat X
        if(in.touching) {
            float targetX = static_cast<float>(in.stylusX) - G.bat.width * 0.5f;
            if(targetX < 0) {
                targetX = 0;
            }
            if(targetX > 320 - G.bat.width) {
                targetX = 320 - G.bat.width;
            }
            G.bat.x = targetX;
        }
        // Update ball(s)
        for(auto &b : G.balls) if(b.active) {
            b.x += b.vx; b.y += b.vy;
            // simple wall bounce
            if(b.x < 0) { b.x = 0; b.vx = -b.vx; }
            if(b.x > 320 - 8) { b.x = 320 - 8; b.vx = -b.vx; }
            if(b.y < 0) { b.y = 0; b.vy = -b.vy; }
            // bottom: reset
            if(b.y > 240) { b.x = G.bat.x + G.bat.width/2 - 4; b.y = G.bat.y - 8; b.vx = 0; b.vy = -1.5f; }
            // bat collision
            if(b.y + 8 >= G.bat.y && b.y <= G.bat.y + G.bat.height && b.x + 8 >= G.bat.x && b.x <= G.bat.x + G.bat.width && b.vy > 0) {
                b.y = G.bat.y - 8; b.vy = -b.vy; // reflect
                float rel = (b.x + 4 - (G.bat.x + G.bat.width/2)) / (G.bat.width/2);
                b.vx = rel * 2.0f; // vary angle
            }
        }
        // Fire (D-Pad Up) could spawn extra balls later
        if(in.fireHeld && G.balls.size() < 3) {
            // simple rate limit not implemented yet
        }
    }

    void render() {
        // Draw bat
        hw_draw_sprite(G.bat.img, G.bat.x, G.bat.y);
        // Draw balls
        for(auto &b : G.balls) if(b.active) hw_draw_sprite(b.img, b.x, b.y);
    }
}

// Public facade
void game_init() { game::init(); }
void game_update(const InputState& in) { game::update(in); }
void game_render() { game::render(); }
