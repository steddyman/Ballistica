// game.cpp - incremental gameplay port (modernizing legacy main.cpp)
#include <vector>
#include <citro2d.h>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include "hardware.hpp"
#include "IMAGE.h"
#include "IMAGE_t3x.h"
#include "sprite_indexes/image_indices.h"
#include "TITLE.h"
#include "BREAK.h"
#include "INSTRUCT.h"
#include "DESIGNER.h"
#include "HIGH.h"
#include "highscores.hpp"
#include "game.hpp"
#include "levels.hpp"
#include "brick.hpp"
#include "SUPPORT.HPP" // legacy constants BATWIDTH, BATHEIGHT, BALLWIDTH, BALLHEIGHT
#include "editor.hpp"
#include "options.hpp"
#include "ui_button.hpp"
#include <vector>
#include <string>
#include "OPTIONS.h"
#include <3ds.h> // for software keyboard

// Basic game state migrated from legacy structures (incremental port)
namespace game
{
    // Global horizontal offset to shift entire play area (bricks already shifted via levels.cpp)
    static constexpr float kPlayfieldOffsetX = 70.0f; // shift right by 70px
    static constexpr float kScreenWidth = 321.0f;
    static constexpr float kInitialBatY = 200.0f;
    static constexpr float kInitialBallY = 180.0f;
    static constexpr float kInitialBallHalf = 4.0f; // legacy center adjust
    // Playfield interior boundaries (logical movement space for bat & balls)
    // Left wall currently aligned with background interior pillar (was 0, now 80).
    static constexpr float kPlayfieldLeftWallX = 80.0f;   // where balls/bat bounce/stop on the left
    static constexpr float kPlayfieldRightWallX = kScreenWidth; // rightmost interior boundary (change if artwork adds gutter)
    static constexpr float kPlayfieldTopWallY = 4.0f;     // top bounce line (param for future lowering)
    
    // UI placement: BONUS indicator stack anchor (left-aligned X), top Y, and vertical gap
    static constexpr int kBonusIndicatorLeftX = 358; // left edge anchor for icons (adjust as needed)
    static constexpr int kBonusIndicatorTopY = 78;    // top Y for the first icon
    static constexpr int kBonusIndicatorGapY = 3;    // gap between icons (in pixels)
    // Title buttons declared later and initialized in init()
    // (Editor UI constants moved to editor.cpp)
    // Fixed geometry (design guarantees these never change now)
    static constexpr int kBrickCols = 13;
    static constexpr int kBrickRows = 11;
    static constexpr int kBrickW = 16;
    static constexpr int kBrickH = 9;
    static constexpr int kBallW = BALLWIDTH;  // 3
    static constexpr int kBallH = BALLHEIGHT; // 3
    struct Ball
    {
        float x, y;
        float vx, vy;
        float px, py;
        bool active;
        C2D_Image img;
    };
    struct Laser
    {
        float x, y;
        bool active;
    };
    struct FallingLetter
    {
        float x, y;   // top-left position
        float vy;     // vertical velocity
        int letter;   // 0=B,1=O,2=N,3=U,4=S
        bool active;
        C2D_Image img;
    };
    struct Bat
    {
        float x, y;
        float width, height;
        C2D_Image img;
    };
    enum class Mode
    {
        Title,
        Playing,
        Editor,
        Options
    };

    // Title buttons
    struct TitleBtn { UIButton btn; Mode next; };
    static TitleBtn kTitleButtons[3];

    struct MovingBrickData
    {
        float pos;
        float dir;
        float minX;
        float maxX;
    };
    struct Particle
    {
        float x, y, vx, vy;
        int life;
        uint32_t color;
    };
    struct BombEvent
    {
        int c;
        int r;
        int frames;
    }; // scheduled bomb trigger

    struct State
    {
        Bat bat{};
        std::vector<Ball> balls;
        std::vector<Laser> lasers;
    std::vector<FallingLetter> letters; // falling BONUS pickups
        C2D_Image imgBall{};
        C2D_Image imgBatNormal{};
        Mode mode = Mode::Title;
    int lives = 3; // changed default lives from 5 to 3
        unsigned long score = 0;
        uint8_t bonusBits = 0; // collected B1..B5 letters
        bool editorLaunched = false;
        float prevBatX = 0.f; // for imparting momentum
    bool ballLocked = false;      // true when awaiting manual launch
    bool prevTouching = false;    // previous frame stylus state to detect release
    // Drag control (relative) for bat movement
    float dragAnchorStylusX = 0.f; // stylus X at start of current drag
    float dragAnchorBatX = 0.f;    // bat X at drag start
    bool dragging = false;         // currently dragging to move bat
    // Level intro overlay
    int levelIntroTimer = 0;       // frames remaining to show level name (generic, not only editor test)
        // Timers/effects
        int reverseTimer = 0;   // frames remaining reverse controls
        int lightsOffTimer = 0; // frames until lights restore
        int murderTimer = 0;    // murderball active
        int laserCharges = 0;   // available shots
        int fireCooldown = 0;   // frames until next laser
        // Moving brick dynamic traversal across contiguous empty span
        std::vector<MovingBrickData> moving; // per-cell data (pos<0 => unused)
        // Particles (bomb / generic)
        std::vector<Particle> particles;
        std::vector<BombEvent> bombEvents; // pending delayed explosions
    };

    static State G;
    static bool g_exitRequested = false;
    bool exit_requested_internal() { return g_exitRequested; }

    void init_assets()
    {
        G.imgBall = hw_image(IMAGE_ball_sprite_idx);
        G.imgBatNormal = hw_image(IMAGE_bat_normal_idx);
        float bw = (G.imgBatNormal.subtex) ? G.imgBatNormal.subtex->width : 64.f;
        float bh = (G.imgBatNormal.subtex) ? G.imgBatNormal.subtex->height : 8.f;
        float batCenterX = kScreenWidth * 0.5f + kPlayfieldOffsetX;
        G.bat = {batCenterX - bw / 2.f, kInitialBatY, bw, bh, G.imgBatNormal};
        float ballStartX = kScreenWidth * 0.5f + kPlayfieldOffsetX - kInitialBallHalf;
    G.balls.push_back({ballStartX, kInitialBallY, 0.0f, 0.f, ballStartX, kInitialBallY, true, G.imgBall});
    G.ballLocked = true;
        char buf[96];
        snprintf(buf, sizeof buf, "bat sprite w=%.1f h=%.1f\n", bw, bh);
        hw_log(buf);
    }

    void init()
    {
        hw_log("game_init\n");
        init_assets();
        levels_load();
        // Initialize title buttons
    // Title buttons sized for 320x240 bottom screen (w/h in pixels)
    kTitleButtons[0].btn.x=60; kTitleButtons[0].btn.y=60; kTitleButtons[0].btn.w=200; kTitleButtons[0].btn.h=24; kTitleButtons[0].btn.label="PLAY"; kTitleButtons[0].btn.color=C2D_Color32(50,50,70,255); kTitleButtons[0].next=Mode::Playing;
    kTitleButtons[1].btn.x=60; kTitleButtons[1].btn.y=100; kTitleButtons[1].btn.w=200; kTitleButtons[1].btn.h=24; kTitleButtons[1].btn.label="EDITOR"; kTitleButtons[1].btn.color=C2D_Color32(50,50,70,255); kTitleButtons[1].next=Mode::Editor;
    kTitleButtons[2].btn.x=60; kTitleButtons[2].btn.y=140; kTitleButtons[2].btn.w=200; kTitleButtons[2].btn.h=24; kTitleButtons[2].btn.label="OPTIONS"; kTitleButtons[2].btn.color=C2D_Color32(50,50,70,255); kTitleButtons[2].next=Mode::Options;
        hw_log("assets loaded\n");
        // Initialize moving brick buffers
        int total = levels_grid_width() * levels_grid_height();
        G.moving.assign(total, {-1.f, 1.f, 0.f, 0.f});
        highscores::init();
    }

    // Title screen configuration (button rectangles)

    // Sequence of sheets to show while idling (fallback to BREAK if others missing)
    struct SeqEntry
    {
        HwSheet sheet;
        int index;
    };
    static const SeqEntry kSequence[] = {
        {HwSheet::Title, TITLE_idx},
        {HwSheet::High, HIGH_idx},
        {HwSheet::Instruct, INSTRUCT_idx}};
    static int seqPos = 0;
    static int seqTimer = 0;                // frames
    static const int kSeqDelayFrames = 300; // ~5s at 60fps

    static void spawn_extra_ball(float x, float y, float vx, float vy)
    {
        if (G.balls.size() >= 8)
            return;
        G.balls.push_back(Ball{x, y, vx, vy, x, y, true, G.imgBall});
    }

    static void spawn_bonus_letter(int letter, float cx, float cy)
    {
        if (letter < 0 || letter > 4)
            return;
        // Spawn the actual BONUS brick sprite as the falling pickup
        int atlasIdx = IMAGE_b_brick_idx;
        switch (letter)
        {
        case 0: atlasIdx = IMAGE_b_brick_idx; break;
        case 1: atlasIdx = IMAGE_o_brick_idx; break;
        case 2: atlasIdx = IMAGE_n_brick_idx; break;
        case 3: atlasIdx = IMAGE_u_brick_idx; break;
        case 4: atlasIdx = IMAGE_s_brick_idx; break;
        }
        C2D_Image img = hw_image(atlasIdx);
        float w = (img.subtex ? img.subtex->width : 10.0f);
        float h = (img.subtex ? img.subtex->height : 11.0f);
        FallingLetter fl{cx - w * 0.5f, cy - h * 0.5f, 0.6f, letter, true, img};
        G.letters.push_back(fl);
    }

    static void apply_brick_effect(BrickType bt, float cx, float cy, Ball &ball)
    {
        switch (bt)
        {
        case BrickType::YB:
            G.score += 10;
            break;
        case BrickType::GB:
            G.score += 20;
            break;
        case BrickType::CB:
            G.score += 30;
            break;
        case BrickType::TB:
            G.score += 40;
            break;
        case BrickType::PB:
            G.score += 50;
            break;
        case BrickType::RB:
            G.score += 100;
            break;
        case BrickType::LB:
            if (G.lives < 99)
                G.lives++;
            break;
        case BrickType::SB:
            ball.vx *= 0.9f;
            ball.vy *= 0.9f;
            break;
        case BrickType::FB:
            ball.vx *= 1.1f;
            ball.vy *= 1.1f;
            break;
        case BrickType::AB:
            spawn_extra_ball(cx, cy, -ball.vx, -std::fabs(ball.vy));
            break;
        case BrickType::T5:
            G.score += 60;
            break; // per-hit score (placeholder)
        case BrickType::BO:
            G.score += 120;
            break; // base bomb score before chain
        case BrickType::RW:
        {
            int cur = levels_current();
            if (levels_count() > 0)
            {
                cur = (cur - 1 + levels_count()) % levels_count();
                levels_set_current(cur);
            }
        }
        break;
        case BrickType::FO:
        {
            int cur = levels_current();
            if (levels_count() > 0)
            {
                cur = (cur + 1) % levels_count();
                levels_set_current(cur);
            }
        }
        break;
        case BrickType::RE:
            G.reverseTimer = 600;
            break; // ~10s
        case BrickType::IS:
            ball.vx *= 0.8f;
            ball.vy *= 0.8f;
            break;
        case BrickType::IF:
            ball.vx *= 1.25f;
            ball.vy *= 1.25f;
            break;
        case BrickType::LA:
            G.laserCharges += 3;
            break;
        case BrickType::MB:
            G.murderTimer = 600;
            break;
        case BrickType::OF:
            G.lightsOffTimer = 600;
            break;
        case BrickType::ON:
            G.lightsOffTimer = 0;
            break;
        case BrickType::BA:
            G.score += 1000;
            break;
        case BrickType::B1: spawn_bonus_letter(0, cx, cy); break;
        case BrickType::B2: spawn_bonus_letter(1, cx, cy); break;
        case BrickType::B3: spawn_bonus_letter(2, cx, cy); break;
        case BrickType::B4: spawn_bonus_letter(3, cx, cy); break;
        case BrickType::B5: spawn_bonus_letter(4, cx, cy); break;
        default:
            break; // others TBD
        }
    // Bonus completion and scoring handled on collection, not at hit time
    }

    static void update_bonus_letters()
    {
        if (G.letters.empty()) return;
        // Compute effective bat collision rectangle (centered logical size)
        float effBatW = (float)BATWIDTH;
        float effBatH = (float)BATHEIGHT;
        float atlasLeft = (G.bat.img.subtex ? G.bat.img.subtex->left : 0.0f);
        float batPadX = (G.bat.width - effBatW) * 0.5f;
        if (batPadX < 0) batPadX = 0;
        float batPadY = (G.bat.height - effBatH) * 0.5f;
        if (batPadY < 0) batPadY = 0;
        float batLeft = G.bat.x + batPadX - atlasLeft;
        float batTop = G.bat.y + batPadY;
        float batRight = batLeft + effBatW;
        float batBottom = batTop + effBatH;
        for (auto &L : G.letters)
        {
            if (!L.active) continue;
            L.y += L.vy;
            L.vy += 0.05f; // gravity
            if (L.y > 240.0f) { L.active = false; continue; }
            float lw = (L.img.subtex ? L.img.subtex->width : 10.0f);
            float lh = (L.img.subtex ? L.img.subtex->height : 11.0f);
            float lLeft = L.x, lTop = L.y, lRight = L.x + lw, lBottom = L.y + lh;
            bool overlap = !(lRight <= batLeft || lLeft >= batRight || lBottom <= batTop || lTop >= batBottom);
            if (overlap)
            {
                // Collect this letter
                switch (L.letter)
                {
                case 0: G.bonusBits |= 0x01; break; // B
                case 1: G.bonusBits |= 0x02; break; // O
                case 2: G.bonusBits |= 0x04; break; // N
                case 3: G.bonusBits |= 0x08; break; // U
                case 4: G.bonusBits |= 0x10; break; // S
                }
                L.active = false;
                // If all five collected, award 250 * level number and reset
                if (G.bonusBits == 0x1F)
                {
                    int lvl = levels_current();
                    int levelNumber = lvl + 1; // levels are 1-based for scoring
                    G.score += 250 * levelNumber;
                    G.bonusBits = 0;
                    hw_log("BONUS COMPLETE (award)\n");
                }
            }
        }
        // Compact inactive periodically
        if (G.letters.size() > 32)
        {
            G.letters.erase(std::remove_if(G.letters.begin(), G.letters.end(), [](const FallingLetter &f){ return !f.active; }), G.letters.end());
        }
    }

    static bool is_moving_type(int raw) { return raw == (int)BrickType::SS || raw == (int)BrickType::SF; }

    static bool bomb_event_scheduled(int c, int r)
    {
        for (const auto &e : G.bombEvents)
        {
            if (e.c == c && e.r == r)
                return true;
        }
        return false;
    }
    static void schedule_neighbor_bombs(int c, int r, int delay)
    {
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                if (dx || dy)
                {
                    int nc = c + dx, nr = r + dy;
                    int raw = levels_brick_at(nc, nr);
                    if (raw == (int)BrickType::BO)
                    {
                        if (!bomb_event_scheduled(nc, nr))
                            G.bombEvents.push_back({nc, nr, delay});
                    }
                }
    }
    static void process_bomb_events()
    {
        if (G.bombEvents.empty())
            return;
        const int ls = levels_left(), ts = levels_top(), cw = levels_brick_width(), ch = levels_brick_height();
        for (auto &e : G.bombEvents)
            if (e.frames > 0)
                --e.frames;
        std::vector<size_t> toExplode;
        toExplode.reserve(G.bombEvents.size());
        for (size_t i = 0; i < G.bombEvents.size(); ++i)
            if (G.bombEvents[i].frames <= 0)
                toExplode.push_back(i);
        for (size_t idx : toExplode)
        {
            if (idx >= G.bombEvents.size())
                continue;
            auto ev = G.bombEvents[idx];
            int raw = levels_brick_at(ev.c, ev.r);
            if (raw != (int)BrickType::BO)
                continue;
            levels_remove_brick(ev.c, ev.r);
            apply_brick_effect(BrickType::BO, ls + ev.c * cw + cw / 2, ts + ev.r * ch + ch / 2, G.balls[0]);
            for (int k = 0; k < 8; k++)
            {
                float angle = (float)k / 8.f * 6.28318f;
                float sp = 0.6f + 0.4f * (k % 4);
                Particle p{(float)(ls + ev.c * cw + cw / 2), (float)(ts + ev.r * ch + ch / 2), std::cos(angle) * sp, std::sin(angle) * sp, 32, C2D_Color32(255, 200, 50, 255)};
                G.particles.push_back(p);
            }
            schedule_neighbor_bombs(ev.c, ev.r, 15); // 15 frame delay
        }
        G.bombEvents.erase(std::remove_if(G.bombEvents.begin(), G.bombEvents.end(), [](const BombEvent &e)
                                          { return e.frames <= 0; }),
                           G.bombEvents.end());
    }

    static void update_moving_bricks(); // fwd
    static void update_bonus_letters(); // fwd

    static void handle_ball_bricks(Ball &ball)
    {
        // Simplified brick collision: treat the ball as a 3x3 box (fixed) and test
        // overlap against bricks (all 16x9) using minimal candidate cell range.
        // We resolve *one* brick per frame (classic behaviour) choosing axis by
        // minimum penetration. No sweeping – velocities are low so tunnelling risk
        // is negligible; can be added later if needed.
        auto detect_and_resolve = [&](bool movingPhase) -> bool
        {
            int ls = levels_left();
            int ts = levels_top();
            float ballL = ball.x;
            float ballT = ball.y;
            float ballR = ball.x + kBallW;
            float ballB = ball.y + kBallH;
            // center values can be derived on demand; avoid unused locals

            if (!movingPhase)
            {
                int minCol = (int)((ballL - ls) / kBrickW);
                if (minCol < 0)
                    minCol = 0;
                if (minCol >= kBrickCols)
                    return false;
                int maxCol = (int)((ballR - 1 - ls) / kBrickW);
                if (maxCol < 0)
                    return false;
                if (maxCol >= kBrickCols)
                    maxCol = kBrickCols - 1;
                int minRow = (int)((ballT - ts) / kBrickH);
                if (minRow < 0)
                    minRow = 0;
                if (minRow >= kBrickRows)
                    return false;
                int maxRow = (int)((ballB - 1 - ts) / kBrickH);
                if (maxRow < 0)
                    return false;
                if (maxRow >= kBrickRows)
                    maxRow = kBrickRows - 1;
                for (int r = minRow; r <= maxRow; ++r)
                    for (int c = minCol; c <= maxCol; ++c)
                    {
                        int raw = levels_brick_at(c, r);
                        if (raw <= 0)
                            continue;
                        if (is_moving_type(raw))
                            continue;
                        float bx = ls + c * kBrickW;
                        float by = ts + r * kBrickH;
                        float br = bx + kBrickW;
                        float bb = by + kBrickH;
                        if (ballR <= bx || ballL >= br || ballB <= by || ballT >= bb)
                            continue;
                        float penLeft = ballR - bx;
                        float penRight = br - ballL;
                        float penTop = ballB - by;
                        float penBottom = bb - ballT;
                        float penX = std::min(penLeft, penRight);
                        float penY = std::min(penTop, penBottom);
                        BrickType bt = (BrickType)raw;
                        bool destroyed = true;
                        if (bt == BrickType::T5)
                            destroyed = levels_damage_brick(c, r);
                        else if (bt == BrickType::BO)
                        {
                            levels_remove_brick(c, r);
                            apply_brick_effect(BrickType::BO, bx + kBrickW * 0.5f, by + kBrickH * 0.5f, ball);
                            for (int k = 0; k < 8; k++)
                            {
                                float angle = (float)k / 8.f * 6.28318f;
                                float sp = 0.6f + 0.4f * (k % 4);
                                Particle p{bx + kBrickW * 0.5f, by + kBrickH * 0.5f, std::cos(angle) * sp, std::sin(angle) * sp, 32, C2D_Color32(255, 200, 50, 255)};
                                G.particles.push_back(p);
                            }
                            schedule_neighbor_bombs(c, r, 15);
                        }
                        else if (bt == BrickType::ID)
                            destroyed = false;
                        else
                            levels_remove_brick(c, r);
                        apply_brick_effect(bt, bx + kBrickW * 0.5f, by + kBrickH * 0.5f, ball);
                        if (G.murderTimer <= 0)
                        {
                            // Seam handling: if we are in a solid band of indestructible bricks and
                            // the collision is near a vertical seam (between two adjacent ID bricks),
                            // prefer a vertical bounce to avoid artificial horizontal reversals.
                            bool preferVertical = false;
                            if (bt == BrickType::ID && penX < penY && std::fabs(ball.vy) >= std::fabs(ball.vx))
                            {
                                // Determine which side we would resolve on horizontally and check neighbor brick
                                bool resolveLeftSide = (penLeft < penRight);
                                int neighborType = 0;
                                if (resolveLeftSide && c > 0)
                                    neighborType = levels_brick_at(c - 1, r);
                                if (!resolveLeftSide && c < kBrickCols - 1)
                                    neighborType = levels_brick_at(c + 1, r);
                                if (neighborType == (int)BrickType::ID)
                                    preferVertical = true; // continuous wall
                            }
                            if (!preferVertical && penX < penY)
                            {
                                if (penLeft < penRight)
                                    ball.x -= penLeft;
                                else
                                    ball.x += penRight;
                                ball.vx = -ball.vx;
                            }
                            else
                            {
                                if (penTop < penBottom)
                                    ball.y -= penTop;
                                else
                                    ball.y += penBottom;
                                ball.vy = -ball.vy;
                            }
                        }
                        if (destroyed && levels_remaining_breakable() == 0 && levels_count() > 0) {
                            if(!editor::test_return_active()) {
                                int next = (levels_current() + 1) % levels_count();
                                levels_set_current(next);
                                hw_log("LEVEL COMPLETE\n");
                            }
                        }
                        return true;
                    }
                return false;
            }
            else
            {
                int ts2 = ts; // only need top offset for moving rows (ls already baked into mb.pos)
                for (int r = 0; r < kBrickRows; ++r)
                    for (int c = 0; c < kBrickCols; ++c)
                    {
                        int raw = levels_brick_at(c, r);
                        if (!is_moving_type(raw))
                            continue;
                        int idx = r * kBrickCols + c;
                        if (idx >= (int)G.moving.size())
                            continue;
                        auto &mb = G.moving[idx];
                        if (mb.pos < 0.f)
                            continue;
                        float bx = mb.pos;
                        float by = ts2 + r * kBrickH;
                        float br = bx + kBrickW;
                        float bb = by + kBrickH;
                        if (ballR <= bx || ballL >= br || ballB <= by || ballT >= bb)
                            continue;
                        BrickType bt = (BrickType)raw;
                        bool destroyed = true;
                        if (bt == BrickType::T5)
                            destroyed = levels_damage_brick(c, r);
                        else if (bt == BrickType::BO)
                        {
                            levels_remove_brick(c, r);
                            apply_brick_effect(BrickType::BO, bx + kBrickW * 0.5f, by + kBrickH * 0.5f, ball);
                            for (int k = 0; k < 8; k++)
                            {
                                float angle = (float)k / 8.f * 6.28318f;
                                float sp = 0.6f + 0.4f * (k % 4);
                                Particle p{bx + kBrickW * 0.5f, by + kBrickH * 0.5f, std::cos(angle) * sp, std::sin(angle) * sp, 32, C2D_Color32(255, 200, 50, 255)};
                                G.particles.push_back(p);
                            }
                            schedule_neighbor_bombs(c, r, 15);
                        }
                        else if (bt == BrickType::ID)
                            destroyed = false;
                        else
                            levels_remove_brick(c, r);
                        apply_brick_effect(bt, bx + kBrickW * 0.5f, by + kBrickH * 0.5f, ball);
                        if (G.murderTimer <= 0)
                        {
                            float penLeft = ballR - bx;
                            float penRight = br - ballL;
                            float penTop = ballB - by;
                            float penBottom = bb - ballT;
                            float penX = std::min(penLeft, penRight);
                            float penY = std::min(penTop, penBottom);
                            bool preferVertical = false;
                            if (bt == BrickType::ID && penX < penY && std::fabs(ball.vy) >= std::fabs(ball.vx))
                            {
                                bool resolveLeftSide = (penLeft < penRight);
                                int neighborType = 0; // moving bricks rarely form continuous walls; still check
                                if (resolveLeftSide && c > 0)
                                    neighborType = levels_brick_at(c - 1, r);
                                if (!resolveLeftSide && c < kBrickCols - 1)
                                    neighborType = levels_brick_at(c + 1, r);
                                if (neighborType == (int)BrickType::ID)
                                    preferVertical = true;
                            }
                            if (!preferVertical && penX < penY)
                            {
                                if (penLeft < penRight)
                                    ball.x -= penLeft;
                                else
                                    ball.x += penRight;
                                ball.vx = -ball.vx;
                            }
                            else
                            {
                                if (penTop < penBottom)
                                    ball.y -= penTop;
                                else
                                    ball.y += penBottom;
                                ball.vy = -ball.vy;
                            }
                        }
                        if (destroyed && levels_remaining_breakable() == 0 && levels_count() > 0) {
                            if(!editor::test_return_active()) {
                                int next = (levels_current() + 1) % levels_count();
                                levels_set_current(next);
                                hw_log("LEVEL COMPLETE\n");
                            }
                        }
                        return true;
                    }
                return false;
            }
        };

        // Sub-step integration to prevent corner tunnelling between adjacent bricks.
        // We subdivide the frame movement if speed exceeds 1px in any axis. On first
        // collision we stop (maintaining one-hit-per-frame behaviour).
        int steps = (int)std::ceil(std::max(std::fabs(ball.vx), std::fabs(ball.vy)));
        if (steps < 1)
            steps = 1;
        if (steps == 1)
        {
            if (detect_and_resolve(false))
                return;                     // static
            (void)detect_and_resolve(true); // moving
            return;
        }
        // Multi-step: start from previous position
        float startX = ball.px;
        float startY = ball.py;
        float totalDX = ball.x - startX;
        float totalDY = ball.y - startY;
        // Reset to start; we will advance in increments
        ball.x = startX;
        ball.y = startY;
        for (int s = 1; s <= steps; ++s)
        {
            float t = (float)s / (float)steps;
            ball.x = startX + totalDX * t;
            ball.y = startY + totalDY * t;
            if (detect_and_resolve(false))
                return;
            if (detect_and_resolve(true))
                return;
        }
    }

    static void update_moving_bricks()
    {
        int cols = levels_grid_width();
        int rows = levels_grid_height();
    int ls = levels_left(); // now includes runtime offset
        int cw = levels_brick_width();
        for (int r = 0; r < rows; ++r)
        {
            for (int c = 0; c < cols; ++c)
            {
                int raw = levels_brick_at(c, r);
                if (!is_moving_type(raw))
                    continue;
                int idx = r * cols + c;
                auto &mb = G.moving[idx];
                int left = c;
                while (left - 1 >= 0 && levels_brick_at(left - 1, r) == 0)
                    --left;
                int right = c;
                while (right + 1 < cols && levels_brick_at(right + 1, r) == 0)
                    ++right;
                float newMin = ls + left * cw;
                float newMax = ls + right * cw;
                bool hadNoSpan = (mb.minX == mb.maxX);
                mb.minX = newMin;
                mb.maxX = newMax;
                if (mb.pos < 0.f)
                    mb.pos = ls + c * cw;
                // If we previously had no span (blocked) and now have space, kick movement on
                if (hadNoSpan && newMin != newMax && mb.dir == 0.f)
                    mb.dir = 1.f;
                // Clamp position into new bounds before advancing
                if (mb.pos < mb.minX)
                    mb.pos = mb.minX;
                if (mb.pos > mb.maxX)
                    mb.pos = mb.maxX;
                float speed = (raw == (int)BrickType::SS) ? 0.5f : 1.0f;
                if (mb.dir != 0.f)
                {
                    mb.pos += mb.dir * speed;
                    if (mb.pos < mb.minX)
                    {
                        mb.pos = mb.minX;
                        mb.dir = 1.f;
                    }
                    else if (mb.pos > mb.maxX)
                    {
                        mb.pos = mb.maxX;
                        mb.dir = -1.f;
                    }
                }
                if (mb.minX == mb.maxX)
                {
                    mb.pos = mb.minX;
                    mb.dir = 0.f;
                }
            }
        }
    }

    static void fire_laser()
    {
        if (G.laserCharges <= 0 || G.fireCooldown > 0)
            return;
        G.laserCharges--;
        G.fireCooldown = 10; // simple cooldown
        G.lasers.push_back({G.bat.x + G.bat.width / 2 - 1, G.bat.y - 4, true});
    }

    static void update_lasers()
    {
        if (G.fireCooldown > 0)
            --G.fireCooldown;
        const int cw = levels_brick_width(), ch = levels_brick_height();
    int ls = levels_left(), ts = levels_top(); // offset-aware
        for (auto &L : G.lasers)
            if (L.active)
            {
                L.y -= 4.0f;
                if (L.y < 0)
                {
                    L.active = false;
                    continue;
                }
                // collision with brick (grid based at laser x,y)
                int col = (int)((L.x - ls) / cw);
                int row = (int)((L.y - ts) / ch);
                if (col >= 0 && col < levels_grid_width() && row >= 0 && row < levels_grid_height())
                {
                    int raw = levels_brick_at(col, row);
                    if (raw > 0)
                    {
                        BrickType bt = (BrickType)raw;
                        if (bt == BrickType::T5)
                        {
                            (void)levels_damage_brick(col, row);
                        }
                        else if (bt == BrickType::BO)
                        {
                            std::vector<DestroyedBrick> list;
                            levels_explode_bomb(col, row, &list);
                            for (auto &db : list)
                                apply_brick_effect((BrickType)db.type, 0, 0, G.balls[0]);
                        }
                        else if (bt == BrickType::ID)
                        { /* indestructible: no action */
                        }
                        else
                            levels_remove_brick(col, row);
                        apply_brick_effect(bt, 0, 0, G.balls[0]);
                        L.active = false;
                    }
                }
            }
    }

    // (Options menu state & logic extracted to options.cpp)

    void update(const InputState &in)
    {
        if (G.mode == Mode::Title)
        {
            // Touch-driven title buttons now trigger on release (press-release inside same button)
            static int sPressedBtn = -1; // index into kTitleButtons while stylus held
            // Physical button mappings: START=Play, SELECT=Editor, X=Exit
            if (in.startPressed)
            {
                levels_set_current(0);
                levels_reset_level(0);
                G.mode = Mode::Playing;
                hw_log("start (START)\n");
                G.prevTouching = in.touching; // keep touch edge tracking consistent
                G.levelIntroTimer = 90; // show intro ~1.5s
                return;
            }
            if (in.selectPressed)
            {
                G.mode = Mode::Editor;
                hw_log("editor (SELECT)\n");
                G.prevTouching = in.touching;
                return;
            }
            if (in.xPressed)
            {
                g_exitRequested = true;
                hw_log("exit (X)\n");
                G.prevTouching = in.touching;
                return;
            }
            if (in.selectPressed)
            {
                G.mode = Mode::Editor;
                hw_log("editor (SELECT)\n");
                G.prevTouching = in.touching;
                return;
            }
            if (in.xPressed)
            {
                g_exitRequested = true;
                hw_log("exit (X)\n");
                G.prevTouching = in.touching;
                return;
            }
            // Handle stylus interactions (press / drag / release)
            if (in.touchPressed)
            {
                sPressedBtn = -1;
                int tx = in.stylusX, ty = in.stylusY;
                for (int i = 0; i < 3; ++i)
                {
                    if (kTitleButtons[i].btn.contains(tx, ty))
                    {
                        sPressedBtn = i;
                        break;
                    }
                }
            }
            // If moving while holding and leave button bounds, cancel
            if (in.touching && sPressedBtn >= 0)
            {
                int tx = in.stylusX, ty = in.stylusY;
                if (!kTitleButtons[sPressedBtn].btn.contains(tx, ty))
                    sPressedBtn = -1; // canceled
            }
            // On release: trigger if still over original button
            if (G.prevTouching && !in.touching)
            {
                if (sPressedBtn >= 0)
                {
                    // We treat release as valid regardless of final coords (optional: require inside)
                    TitleBtn &tb = kTitleButtons[sPressedBtn];
                    if (tb.next == Mode::Playing)
                    {
                        levels_set_current(0);
                        levels_reset_level(0);
                        G.levelIntroTimer = 90;
                        G.letters.clear();
                    }
                    if (tb.next == Mode::Options)
                        options::begin();
                    G.mode = tb.next;
                    // Ensure touch edge bookkeeping so first playing frame doesn't consider earlier press
                    G.prevTouching = false;
                    sPressedBtn = -1;
                    return;
                }
            }
            // Cycle sequence if user idle
            if (++seqTimer > kSeqDelayFrames)
            {
                seqTimer = 0;
                seqPos = (seqPos + 1) % (int)(sizeof(kSequence) / sizeof(kSequence[0]));
            }
            G.prevTouching = in.touching; // update before early return
            return;
        }
        if (G.mode == Mode::Options)
        {
            options::Action act = options::update(in);
            if (act == options::Action::ExitToTitle) { G.mode = Mode::Title; return; }
            if (act == options::Action::SaveAndExit) { G.mode = Mode::Title; return; }
            if (in.selectPressed) { G.mode = Mode::Title; hw_log("options->title (SELECT)\n"); return; }
            return;
        }
        if (G.mode == Mode::Editor)
        {
            editor::EditorAction act = editor::update(in);
            if (act == editor::EditorAction::StartTest) {
                // Initialize a fresh play session specifically for editor test runs
                G.balls.clear();
                {
                    float ballStartX = kScreenWidth * 0.5f + kPlayfieldOffsetX - kInitialBallHalf;
                    G.balls.push_back({ballStartX, kInitialBallY, 0.0f, 0.f, ballStartX, kInitialBallY, true, G.imgBall});
                }
                G.ballLocked = true;
                G.lives = 3; // ensure lives reset (updated default)
                G.score = 0;
                G.bonusBits = 0;
                G.reverseTimer = G.lightsOffTimer = G.murderTimer = 0;
                G.laserCharges = 0;
                G.fireCooldown = 0;
                G.letters.clear();
                // Reinitialize moving bricks data for current layout
                int totalCells = levels_grid_width() * levels_grid_height();
                G.moving.assign(totalCells, {-1.f, 1.f, 0.f, 0.f});
                hw_log("TEST init session\n");
                G.levelIntroTimer = 90; // reuse generic intro timer (editor fade overlay still draws if active)
                G.mode = Mode::Playing; return; }
            if (act == editor::EditorAction::SaveAndExit) { G.mode = Mode::Title; return; }
            if (in.selectPressed) { editor::persist_current_level(); G.mode = Mode::Title; return; }
            return;
        }
#ifdef __3DS__
        // In play mode, if we are in a test session launched from editor and level already cleared, return immediately.
        if (G.mode == Mode::Playing && editor::test_return_active() && !editor::test_grace_active() && levels_remaining_breakable()==0) {
            levels_reset_level(editor::current_level_index());
            editor::on_return_from_test_full();
            G.mode = Mode::Editor;
            hw_log("TEST auto-return (pre-play breakables=0)\n");
            return;
        }
#endif
#if defined(DEBUG) && DEBUG
        // Debug level switching (L previous, R next)
        if (in.levelPrevPressed || in.levelNextPressed)
        {
            int cur = levels_current();
            int total = levels_count();
            if (in.levelPrevPressed && cur > 0)
                cur--;
            if (in.levelNextPressed && cur < total - 1)
                cur++;
            if (cur != levels_current())
            {
                levels_set_current(cur);
                levels_reset_level(cur);
                // Reset game state (fresh start)
                G.balls.clear();
                {
                    float ballStartX = kScreenWidth * 0.5f + kPlayfieldOffsetX - kInitialBallHalf;
                    G.balls.push_back(Ball{ballStartX, kInitialBallY, 0.0f, 0.f, ballStartX, kInitialBallY, true, G.imgBall});
                }
                G.ballLocked = true;
                G.lives = 3; // reset lives when starting from Title
                G.score = 0;
                G.bonusBits = 0;
                G.reverseTimer = G.lightsOffTimer = G.murderTimer = 0;
                G.laserCharges = 0;
                G.fireCooldown = 0;
                G.letters.clear();
                // Re-init moving brick arrays for new layout
                int totalCells = levels_grid_width() * levels_grid_height();
                G.moving.assign(totalCells, {-1.f, 1.f, 0.f, 0.f});
                hw_log("DEBUG: level switched\n");
                G.levelIntroTimer = 90;
            }
        }
#endif
        // Process bomb chain before physics so collisions see updated board
        process_bomb_events();
        // Stylus drag controls bat X (relative). A tap elsewhere without drag no longer teleports the bat.
        if (in.touchPressed)
        {
            G.dragging = true;
            G.dragAnchorStylusX = (float)in.stylusX;
            G.dragAnchorBatX = G.bat.x;
        }
        if (!in.touching)
        {
            G.dragging = false; // end drag on release
        }
        if (G.dragging && in.touching)
        {
            G.prevBatX = G.bat.x; // record before applying delta
            float curStylusX = (float)in.stylusX;
            float dx = curStylusX - G.dragAnchorStylusX;
            if (G.reverseTimer > 0)
                dx = -dx; // reverse control effect
            float targetX = G.dragAnchorBatX + dx;
            if (targetX < kPlayfieldLeftWallX)
                targetX = kPlayfieldLeftWallX;
            if (targetX > kPlayfieldRightWallX - G.bat.width)
                targetX = kPlayfieldRightWallX - G.bat.width;
            G.bat.x = targetX;
        }
        if (in.fireHeld)
            fire_laser();
        update_lasers();
        if (G.reverseTimer > 0)
            --G.reverseTimer;
        if (G.lightsOffTimer > 0)
            --G.lightsOffTimer;
        if (G.murderTimer > 0)
            --G.murderTimer;
        update_moving_bricks();
    update_bonus_letters();
        // Update particles
        for (auto &p : G.particles)
        {
            if (p.life <= 0)
                continue;
            p.x += p.vx;
            p.y += p.vy;
            p.vy += 0.02f; // slight gravity
            p.life--;
        }
        // Compact occasionally
        if ((int)G.particles.size() > 256)
        {
            G.particles.erase(std::remove_if(G.particles.begin(), G.particles.end(), [](const Particle &p)
                                             { return p.life <= 0; }),
                              G.particles.end());
        }
        // Update ball(s)
        for (auto &b : G.balls)
            if (b.active)
            {
                // If primary ball is locked, keep it attached to bat and skip physics
                        if (&b == &G.balls[0] && G.ballLocked)
                        {
                            b.x = G.bat.x + G.bat.width * 0.5f - kBallW * 0.5f;
                            b.y = G.bat.y - kBallH - 1; // small gap
                            b.px = b.x;
                            b.py = b.y;
                            b.vx = 0.f;
                            b.vy = 0.f;
                            continue; // parked
                        }
                b.px = b.x;
                b.py = b.y;
                b.x += b.vx;
                b.y += b.vy;
                // simple wall bounce using parametric playfield bounds.
                if (b.x < kPlayfieldLeftWallX)
                {
                    b.x = kPlayfieldLeftWallX;
                    b.vx = -b.vx;
                }
                if (b.x > kPlayfieldRightWallX - kBallW)
                {
                    b.x = kPlayfieldRightWallX - kBallW;
                    b.vx = -b.vx;
                }
                if (b.y < kPlayfieldTopWallY)
                {
                    b.y = kPlayfieldTopWallY;
                    b.vy = -b.vy;
                }
                // bottom: lose life
                if (b.y > 240)
                {
                    // Only lose a life if this was the last active ball.
                    int activeCount = 0;
                    for (const auto &bb : G.balls)
                        if (bb.active)
                            ++activeCount;
                    if (activeCount > 1)
                    {
                        // Deactivate this ball; others continue. No life lost.
                        b.active = false;
                        continue;
                    }
                    G.lives--;
                    if (G.lives <= 0) {
                        if (editor::test_return_active()) {
                            // Return to editor instead of title/highscore flow
                            levels_reset_level(editor::current_level_index()); // restore snapshot
                            editor::on_return_from_test_full();
                            G.mode = Mode::Editor;
                            return;
                        }
                        hw_log("GAME OVER\n");
                        int levelReached = levels_current() + 1;
                        int pos = highscores::submit(G.score, levelReached);
                        if (pos >= 0) {
#ifdef PLATFORM_3DS
                            {
                                SwkbdState swkbd;
                                char name[highscores::MAX_NAME + 1] = "";
                                swkbdInit(&swkbd, SWKBD_TYPE_QWERTY, 1, highscores::MAX_NAME);
                                swkbdSetHintText(&swkbd, "Name");
                                if (swkbdInputText(&swkbd, name, sizeof(name)) == SWKBD_BUTTON_RIGHT)
                                    highscores::set_name(pos, name);
                                else
                                    highscores::set_name(pos, "PLAYER");
                                highscores::save();
                            }
#endif
                            seqPos = 1;
                            seqTimer = 0; // jump to High screen
                        }
                        G.mode = Mode::Title;
                        levels_set_current(0);
                        levels_reset_level(0);
                        G.balls.clear();
                        {
                            float ballStartX = kScreenWidth * 0.5f + kPlayfieldOffsetX - kInitialBallHalf;
                            G.balls.push_back({ballStartX, kInitialBallY, 0.0f, 0.f, ballStartX, kInitialBallY, true, G.imgBall});
                        }
                        G.ballLocked = true;
                        G.lives = 3; // reset lives after returning to title
                        G.score = 0;
                        G.bonusBits = 0;
                        G.reverseTimer = G.lightsOffTimer = G.murderTimer = 0;
                        G.laserCharges = 0;
                        G.fireCooldown = 0;
                        G.letters.clear();
                        return;
                    }
                    b.x = G.bat.x + G.bat.width * 0.5f - kBallW * 0.5f;
                    b.y = G.bat.y - kBallH - 1;
                    b.px = b.x;
                    b.py = b.y;
                    b.vx = 0;
                    b.vy = 0.f;
                    if (&b == &G.balls[0])
                        G.ballLocked = true; // relock primary ball after life loss
                    continue;
                }
                // Bat collision using legacy logical sizes (BATWIDTH/BATHEIGHT, BALLWIDTH/BALLHEIGHT)
                if (b.vy > 0)
                {
                    constexpr float ballCollW = (float)BALLWIDTH;  // legacy collision width
                    constexpr float ballCollH = (float)BALLHEIGHT; // legacy collision height
                    // Align logical collision box centered within the ACTUAL rendered sprite (handles atlas trims)
                    float spriteW = (b.img.subtex ? b.img.subtex->width : 8.f);
                    float spriteH = (b.img.subtex ? b.img.subtex->height : 8.f);
                    float ballCenterX = b.x + spriteW * 0.5f;
                    float ballCenterYPrev = b.py + spriteH * 0.5f;
                    float ballCenterY = b.y + spriteH * 0.5f;
                    float ballHalfW = ballCollW * 0.5f;
                    float ballHalfH = ballCollH * 0.5f;
                    float ballBottomPrev = ballCenterYPrev + ballHalfH;
                    float ballBottom = ballCenterY + ballHalfH;
                    // Bat effective rectangle: shrink sprite to legacy BATWIDTH/BATHEIGHT centered
                    float effBatW = (float)BATWIDTH;
                    float effBatH = (float)BATHEIGHT;
                    float batPadX = (G.bat.width - effBatW) * 0.5f;
                    if (batPadX < 0)
                        batPadX = 0;
                    // Center the logical (legacy) bat height within the rendered sprite height
                    float batPadY = (G.bat.height - effBatH) * 0.5f;
                    if (batPadY < 0)
                        batPadY = 0;
                    float batTop = G.bat.y + batPadY; // logical top surface
                    float batLeft = G.bat.x + batPadX;
                    // Full sprite bounds for broad-phase (avoid off-by-one visual mismatch)
                    float atlasLeft = (G.bat.img.subtex ? G.bat.img.subtex->left : 0.0f);
                    float fullLeft = G.bat.x - atlasLeft;
                    float fullRight = fullLeft + G.bat.width;
                    // Detect crossing of the bat top line this frame (sweep)
                    bool horizOverlap = (ballCenterX + ballHalfW) > fullLeft && (ballCenterX - ballHalfW) < fullRight;
                    bool crossedTop = (ballBottomPrev <= batTop && ballBottom >= batTop);
                    // Extra sweep: if moving fast diagonally, the bottom may skip the exact top line but center enters vertical span
                    bool enteredFromAbove = (ballCenterYPrev <= batTop && (ballCenterY + ballHalfH) >= batTop * 0.999f);
                    if (horizOverlap && (crossedTop || enteredFromAbove))
                    {
                        // Place ball just above logical top using full rendered sprite alignment
                        float adjust = (ballBottom - batTop);
                        b.y -= adjust; // shift up so that logical bottom sits on top line
                        b.vy = -b.vy;
                        // Angle based on horizontal offset inside effective bat width
                        float rel = (ballCenterX - (batLeft + effBatW * 0.5f)) / (effBatW * 0.5f);
                        if (rel < -1.f)
                            rel = -1.f;
                        if (rel > 1.f)
                            rel = 1.f;
                        // Base horizontal component from relative position
                        float baseVX = rel * 2.0f;
                        // Add momentum imparted by bat movement this frame
                        float batDX = G.bat.x - G.prevBatX;
                        baseVX += batDX * 0.08f; // tuning factor from legacy feel approximation
                        // Clamp to reasonable range
                        if (baseVX < -3.f)
                            baseVX = -3.f;
                        if (baseVX > 3.f)
                            baseVX = 3.f;
                        b.vx = baseVX;
                    }
                }
                handle_ball_bricks(b);
            }
        // Fire (D-Pad Up) could spawn extra balls later
        if (in.fireHeld && G.balls.size() < 3)
        {
            // simple rate limit not implemented yet
        }

        // Manual launch: require a touch THEN release after lock (even if not touching when ball appears)
        if (G.ballLocked)
        {
            // Only allow launching if we have seen a touch while locked and now see its release.
            // prevTouching tracks global previous frame, but we only care about transitions that happen after lock.
            static bool sawTouchWhileLocked = false; // function-static: persists across frames; reset when unlocked
            if (!sawTouchWhileLocked && in.touching)
                sawTouchWhileLocked = true; // first touch since lock
            bool released = sawTouchWhileLocked && G.prevTouching && !in.touching; // release of that touch
            if (released)
            {
                if (!G.balls.empty())
                {
                    Ball &b0 = G.balls[0];
                    b0.vy = -1.5f;
                    float batDX = G.bat.x - G.prevBatX;
                    b0.vx = batDX * 0.15f;
                }
                G.ballLocked = false;
                sawTouchWhileLocked = false; // reset for next time we lock
            }
            if (!G.ballLocked)
                sawTouchWhileLocked = false; // safety
        }
        G.prevTouching = in.touching;
    }

    void render()
    {
    // Ensure correct brick horizontal offset per mode (editor unshifted, gameplay shifted)
    int desiredOffset = 0;
    if (G.mode == Mode::Playing) desiredOffset = (int)kPlayfieldOffsetX; // gameplay shift
    if (levels_get_draw_offset() != desiredOffset) levels_set_draw_offset(desiredOffset);
        if (G.mode == Mode::Title)
        {
            // Draw current sequence image (skip if not loaded, fallback attempts)
            const SeqEntry &cur = kSequence[seqPos];
            C2D_Image img = hw_image_from(cur.sheet, cur.index);
            if (!img.tex)
            { // simple fallback chain
                for (auto &alt : kSequence)
                {
                    img = hw_image_from(alt.sheet, alt.index);
                    if (img.tex)
                        break;
                }
            }
            if (img.tex)
                hw_draw_sprite(img, 0, 0);
            if (kSequence[seqPos].sheet == HwSheet::High)
            {
                const highscores::Entry *tab = highscores::table();
                int shown = highscores::NUM_SCORES;
                if (shown > 10)
                    shown = 10;
                for (int i = 0; i < shown; i++)
                {
                    char nameTrunc[highscores::MAX_NAME + 1];
                    std::strncpy(nameTrunc, tab[i].name, highscores::MAX_NAME);
                    nameTrunc[highscores::MAX_NAME] = '\0';
                    char line[64];
                    snprintf(line, sizeof line, "%2d  %7lu  L%02d  %-10s", i + 1, (unsigned long)tab[i].score, tab[i].level, nameTrunc);
                    int baseX = 32;
                    int baseY = 48 + i * 18;
                    float scale = 2.0f; // 10% smaller than previous 2.0
                    // Optimised merged-run shadow text (significantly fewer draw objects)
                    hw_draw_text_shadow_scaled(baseX, baseY, line, 0xFFFFFFFF, 0x000000FF, scale);
                }
                // NOTE: Logs are drawn in entry_3ds when toggled; no need here unless we add a param.
            }
            return;
        }
        // Gameplay background (BREAK.png) - draw first so overlays appear above
        if (G.mode == Mode::Playing) {
            C2D_Image bg = hw_image_from(HwSheet::Break, BREAK_idx);
            if (bg.tex) hw_draw_sprite(bg, 0, 0);
        }
        // Editor-specific fade overlay still displays if active
        if (G.mode == Mode::Playing && editor::fade_overlay_active())
            editor::render_fade_overlay();
        // Generic per-level intro (shows level name every level start)
        if (G.mode == Mode::Playing && G.levelIntroTimer > 0)
        {
            C2D_DrawRectSolid(0,0,0,320,240,C2D_Color32(0,0,0,120));
            const char *nm = levels_get_name(levels_current());
            if (!nm) nm = "Level";
            float scale = 2.0f; // double size
            int tw = hw_text_width(nm);
            int x = (int)((320 - tw * scale) * 0.5f);
            int y = 110 + 24; // moved down 24px
            hw_draw_text_shadow_scaled(x, y, nm, 0xFFFFFFFF, 0x000000FF, scale);
            --G.levelIntroTimer;
        }
        if (G.mode == Mode::Editor)
        {
            editor::render();
            return;
        }
    if (G.mode == Mode::Options) { options::render(); return; }
        // Render static bricks first then overwrite dynamic moving bricks at their current x
        levels_render();
        int cols = levels_grid_width();
        int rows = levels_grid_height();
    int ls = levels_left(); // offset-aware
        int ts = levels_top();
        int cw = levels_brick_width();
        int ch = levels_brick_height();
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
            {
                int raw = levels_brick_at(c, r);
                if (!is_moving_type(raw))
                    continue;
                int idx = r * cols + c;
                int atlas = levels_atlas_index(raw);
                if (atlas < 0)
                    continue;
                float y = ts + r * ch; // clear original cell to avoid ghost
                C2D_DrawRectSolid(ls + c * cw, y, 0, cw, ch, C2D_Color32(0, 0, 0, 255));
                float x = (idx < (int)G.moving.size() && G.moving[idx].pos >= 0.f) ? G.moving[idx].pos : (ls + c * cw);
                if (idx < (int)G.moving.size())
                {
                    if (x < G.moving[idx].minX)
                        x = G.moving[idx].minX;
                    if (x > G.moving[idx].maxX)
                        x = G.moving[idx].maxX;
                }
                hw_draw_sprite(hw_image(atlas), x, y);
                if (raw == (int)BrickType::T5)
                {
                    int hp = levels_brick_hp(c, r);
                    if (hp > 0)
                    {
                        // darker overlay increases with damage (hp ranges 1..5). When hp=5 (full) no overlay; hp=1 heavy overlay.
                        int missing = 5 - hp;
                        int alpha = 30 + missing * 40;
                        if (alpha > 180)
                            alpha = 180;
                        C2D_DrawRectSolid(x, y, 0, cw, ch, C2D_Color32(255, 0, 0, (uint8_t)alpha));
                    }
                }
            }
#if defined(DEBUG) && DEBUG
        // Draw static brick colliders (red outlines)
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
            {
                int raw = levels_brick_at(c, r);
                if (raw <= 0)
                    continue;
                if (is_moving_type(raw))
                    continue;
                float bx = ls + c * cw;
                float by = ts + r * ch;
                C2D_DrawRectSolid(bx, by, 0, cw, 1, C2D_Color32(255, 0, 0, 200));          // top
                C2D_DrawRectSolid(bx, by + ch - 1, 0, cw, 1, C2D_Color32(255, 0, 0, 80));  // bottom
                C2D_DrawRectSolid(bx, by, 0, 1, ch, C2D_Color32(255, 0, 0, 120));          // left
                C2D_DrawRectSolid(bx + cw - 1, by, 0, 1, ch, C2D_Color32(255, 0, 0, 120)); // right
            }
        // Draw moving brick colliders (solid red border)
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
            {
                int raw = levels_brick_at(c, r);
                if (!is_moving_type(raw))
                    continue;
                int idx = r * cols + c;
                if (idx >= (int)G.moving.size())
                    continue;
                if (G.moving[idx].pos < 0.f)
                    continue;
                float x = G.moving[idx].pos;
                float y = ts + r * ch;
                C2D_DrawRectSolid(x, y, 0, cw, 1, C2D_Color32(255, 0, 0, 200));
                C2D_DrawRectSolid(x, y + ch - 1, 0, cw, 1, C2D_Color32(255, 0, 0, 80));
                C2D_DrawRectSolid(x, y, 0, 1, ch, C2D_Color32(255, 0, 0, 120));
                C2D_DrawRectSolid(x + cw - 1, y, 0, 1, ch, C2D_Color32(255, 0, 0, 120));
            }
#endif
        // Simple particles
        for (auto &p : G.particles)
        {
            if (p.life <= 0)
                continue;
            C2D_DrawRectSolid(p.x, p.y, 0, 2, 2, p.color);
        }
        // Draw falling BONUS letters
        for (auto &L : G.letters)
            if (L.active)
                hw_draw_sprite(L.img, L.x, L.y);
        // HUD overlay
        char hud[128];
        char bonus[8];
        int bi = 0;
        if (G.bonusBits & 0x01)
            bonus[bi++] = 'B';
        if (G.bonusBits & 0x02)
            bonus[bi++] = 'O';
        if (G.bonusBits & 0x04)
            bonus[bi++] = 'N';
        if (G.bonusBits & 0x08)
            bonus[bi++] = 'U';
        if (G.bonusBits & 0x10)
            bonus[bi++] = 'S';
        bonus[bi] = '\0';
        snprintf(hud, sizeof hud, "L%02d SCO:%lu LIVES:%d LAS:%d %s%s%s", levels_current() + 1, G.score, G.lives, G.laserCharges,
                 (G.reverseTimer > 0 ? "REV " : ""), (G.murderTimer > 0 ? "MB " : ""), bonus);
        hw_draw_text(4, 4, hud, 0xFFFFFFFF);
        if (G.lightsOffTimer > 0)
        {
            // dark overlay (draw first so HUD stays bright) - simple approach: semi-transparent rect
            C2D_DrawRectSolid(0, 0, 0, 320, 240, C2D_Color32(0, 0, 0, 140));
        }
    // BONUS indicators: draw five letter icons at right side, dim when not collected
        {
            const int iconIdx[5] = { IMAGE_letterb_idx, IMAGE_lettero_idx, IMAGE_lettern_idx, IMAGE_letteru_idx, IMAGE_letters_idx };
            for (int i = 0; i < 5; ++i)
            {
                C2D_Image img = hw_image(iconIdx[i]);
                float w = (img.subtex ? img.subtex->width : 10.0f);
                float h = (img.subtex ? img.subtex->height : 11.0f);
                int drawX = kBonusIndicatorLeftX; // left-align to anchor
        int drawY = kBonusIndicatorTopY + i * ((int)h + kBonusIndicatorGapY);
                hw_draw_sprite(img, (float)drawX, (float)drawY);
                if ((G.bonusBits & (1 << i)) == 0)
                    C2D_DrawRectSolid(drawX, drawY, 0, w, h, C2D_Color32(0, 0, 0, 140)); // dim overlay
            }
        }
        // TODO: overlay HUD (score/lives/bonus) using tiny font logger or future UI layer
        // Draw bat (account for atlas left trim so column 0 visible)
        float batAtlasLeft = (G.bat.img.subtex ? G.bat.img.subtex->left : 0.0f);
        float batDrawX = G.bat.x - batAtlasLeft;
        hw_draw_sprite(G.bat.img, batDrawX, G.bat.y);
#if defined(DEBUG) && DEBUG
        // Draw logical bat collision rectangle (centered reduced width & height)
        float effBatW = (float)BATWIDTH;
        float effBatH = (float)BATHEIGHT;
        float batPadX = (G.bat.width - effBatW) * 0.5f;
        if (batPadX < 0)
            batPadX = 0;
        // Match centered padding logic from collision code
        float batPadY = (G.bat.height - effBatH) * 0.5f;
        if (batPadY < 0)
            batPadY = 0;
        float batLeft = G.bat.x + batPadX - batAtlasLeft;
        float batTop = G.bat.y + batPadY;
        C2D_DrawRectSolid(batLeft, batTop, 0, effBatW, 1, C2D_Color32(255, 0, 0, 180));              // top line
        C2D_DrawRectSolid(batLeft, batTop + effBatH - 1, 0, effBatW, 1, C2D_Color32(255, 0, 0, 80)); // bottom line
        C2D_DrawRectSolid(batLeft, batTop, 0, 1, effBatH, C2D_Color32(255, 0, 0, 80));               // left
        C2D_DrawRectSolid(batLeft + effBatW - 1, batTop, 0, 1, effBatH, C2D_Color32(255, 0, 0, 80)); // right
        // Draw ball logical footprint(s)
        for (auto &b : G.balls)
            if (b.active)
            {
                float spriteW = (b.img.subtex ? b.img.subtex->width : 8.f);
                float spriteH = (b.img.subtex ? b.img.subtex->height : 8.f);
                float ballCenterX = b.x + spriteW * 0.5f;
                float ballCenterY = b.y + spriteH * 0.5f;
                float halfW = BALLWIDTH * 0.5f;
                float halfH = BALLHEIGHT * 0.5f;
                float lx = ballCenterX - halfW;
                float ly = ballCenterY - halfH;
                C2D_DrawRectSolid(lx, ly, 0, BALLWIDTH, BALLHEIGHT, C2D_Color32(0, 255, 0, 90));
            }
#endif
        // Draw balls (multi-hit bricks visual tweak: overlay HP number optional later)
        for (auto &b : G.balls)
            if (b.active)
                hw_draw_sprite(b.img, b.x, b.y);
        // Draw lasers
        for (auto &L : G.lasers)
            if (L.active)
                C2D_DrawRectSolid(L.x, L.y, 0, 2, 6, C2D_Color32(255, 255, 100, 255));
        // If in test mode (editor launched) and level ended (no breakables) or lives depleted, return to editor
        if (editor::test_return_active() && G.mode == Mode::Playing)
        {
            bool levelDone = (!editor::test_grace_active() && levels_remaining_breakable() == 0);
            bool livesGone = (G.lives <= 0);
            if (levelDone || livesGone)
            {
                G.mode = Mode::Editor;
                levels_set_current(editor::current_level_index());
                editor::on_return_from_test_full();
                hw_log("TEST return (levelDone or livesGone)\n");
            }
        }
        // Tick grace after all logic
        if (editor::test_grace_active()) editor::tick_test_grace();
    }
}

// Public facade
void game_init() { game::init(); }
void game_update(const InputState &in) { game::update(in); }
void game_render() { game::render(); }
void game_render_title_buttons(const InputState &in) {
    using namespace game;
    if (G.mode != Mode::Title) return;
    // Draw the three title buttons (PLAY/EDITOR/OPTIONS) using bottom-screen coordinate system.
    for (auto &tb : kTitleButtons) {
        bool pressed = in.touching && tb.btn.contains(in.stylusX, in.stylusY);
        ui_draw_button(tb.btn, pressed);
    }
}
GameMode game_mode()
{
    using namespace game;
    switch (G.mode)
    {
    case game::Mode::Title:
        return GameMode::Title;
    case game::Mode::Playing:
        return GameMode::Playing;
    case game::Mode::Editor:
        return GameMode::Editor;
    case game::Mode::Options:
        return GameMode::Options;
    }
    return GameMode::Title;
}
bool exit_requested() { return game::exit_requested_internal(); }
