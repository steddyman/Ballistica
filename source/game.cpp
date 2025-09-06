#include <vector>
#include <algorithm>
#include <cstdint>
#include "hardware.hpp"
#include "IMAGE.h"
#include "IMAGE_t3x.h"
#include "sprite_indexes/image_indices.h"
#include "TITLE.h"
#include "BREAK.h"
#include "INSTRUCT.h"
#include "DESIGNER.h"
#include "game.hpp"

// Forward decl (implemented in levels.cpp)
void levels_load();
void levels_render();

// Basic game state migrated from legacy structures (incremental port)
namespace game {
    struct Ball { float x, y; float vx, vy; bool active; C2D_Image img; };
    struct Bat  { float x, y; float width, height; C2D_Image img; };
    enum class Mode { Title, Playing, Editor };

    struct State {
        Bat bat{};
        std::vector<Ball> balls;
        C2D_Image imgBall{};
        C2D_Image imgBatNormal{};
        Mode mode = Mode::Title;
    bool editorLaunched = false; // placeholder (not used now)
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
        levels_load();
    hw_log("assets loaded\n");
    }

    // Title screen configuration (button rectangles)
    static const struct { int x,y,w,h; Mode next; } kTitleButtons[] = {
        {16,28,22,12, Mode::Playing}, // New Game
        {16,39,22,12, Mode::Editor}   // Edit Levels
    };

    // Sequence of sheets to show while idling (fallback to BREAK if others missing)
    struct SeqEntry { HwSheet sheet; int index; };
    static const SeqEntry kSequence[] = {
        {HwSheet::Title, TITLE_idx},
        {HwSheet::Instruct, INSTRUCT_idx},
        {HwSheet::Break, BREAK_idx}
    };
    static int seqPos = 0;
    static int seqTimer = 0; // frames
    static const int kSeqDelayFrames = 180; // ~3s at 60fps

    void update(const InputState& in) {
    // Removed per-frame heartbeat log to prevent console spam and log flooding.
        if(G.mode == Mode::Title) {
            // If touch, check hit against buttons
            if(in.touching) {
                for(auto &b : kTitleButtons) {
                    if(in.stylusX >= b.x && in.stylusX < b.x + b.w &&
                       in.stylusY >= b.y && in.stylusY < b.y + b.h) {
                        G.mode = b.next;
                        hw_log(b.next==Mode::Playing?"start\n":"editor\n");
                        return;
                    }
                }
            }
            // Cycle sequence if user idle
            if(++seqTimer > kSeqDelayFrames) { seqTimer = 0; seqPos = (seqPos + 1) % (int)(sizeof(kSequence)/sizeof(kSequence[0])); }
            return;
        }
    if(G.mode == Mode::Editor) { return; }
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
        if(G.mode == Mode::Title) {
            // Draw current sequence image (skip if not loaded, fallback attempts)
            const SeqEntry& cur = kSequence[seqPos];
            C2D_Image img = hw_image_from(cur.sheet, cur.index);
            if(!img.tex) { // simple fallback chain
                for(auto &alt : kSequence) { img = hw_image_from(alt.sheet, alt.index); if(img.tex) break; }
            }
            if(img.tex) hw_draw_sprite(img, 0, 0);
            return;
        }
        if(G.mode == Mode::Editor) {
            C2D_Image img = hw_image_from(HwSheet::Designer, DESIGNER_idx);
            if(img.tex) hw_draw_sprite(img, 0, 0); else {
                img = hw_image_from(HwSheet::Instruct, INSTRUCT_idx);
                if(img.tex) hw_draw_sprite(img, 0, 0);
            }
            return;
        }
        // Draw bat
        levels_render(); // draw bricks behind entities
        hw_draw_sprite(G.bat.img, G.bat.x, G.bat.y);
        // Draw balls
        for(auto &b : G.balls) if(b.active) hw_draw_sprite(b.img, b.x, b.y);
    }
}

// Public facade
void game_init() { game::init(); }
void game_update(const InputState& in) { game::update(in); }
void game_render() { game::render(); }
