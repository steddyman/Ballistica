
#include <cstdint>
#include <cmath>
#include <citro2d.h>

// ...existing code...


// ...existing code...

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
#include "BACKGROUND.h"
#include "highscores.hpp"
#include "game.hpp"
#include "sound.hpp"
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
    // Dual-screen alignment: use a 320px logical width on both screens.
    // On the top screen (400px wide), add a +40px X offset so content aligns horizontally.
    // Playfield interior boundaries (logical movement space for bat & balls)
    // Left wall currently aligned with background interior pillar (was 0, now 80).
    #include "layout.hpp"
    static constexpr int kTopXOffset = layout::TOP_X_OFFSET;
    static constexpr float kPlayfieldOffsetX = 0.0f;
    static constexpr float kScreenWidth = layout::SCREEN_WIDTH;
    static constexpr float kPlayfieldLeftWallX = layout::PLAYFIELD_LEFT_WALL_X;
    static constexpr float kPlayfieldRightWallX = layout::PLAYFIELD_RIGHT_WALL_X;
    static constexpr float kPlayfieldTopWallY = layout::PLAYFIELD_TOP_WALL_Y;
    static constexpr float kInitialBatY = layout::kInitialBatY;
    static constexpr float kInitialBallY = layout::kInitialBallY;
    static constexpr float kInitialBallHalf = layout::kInitialBallHalf;
    static constexpr int   kBarrierGlowFrames = 18; // frames to show the barrier hit glow
    
    // UI placement for BONUS indicators: top row Y and inter-icon gap
    static constexpr int kBonusIndicatorTopY = 4;   // top row alongside HUD
    static constexpr int kBonusIndicatorGapY = 3;   // gap between icons (pixels); reused horizontally
    // Title buttons declared later and initialized in init()
    // (Editor UI constants moved to editor.cpp)
    // Fixed geometry (design guarantees these never change now)
    static constexpr int kBrickCols = 13;
    static constexpr int kBrickRows = 11;
    // Brick cell size is sourced from levels.cpp getters; keep constants only as defaults.
    static constexpr int kBrickW = 16;
    static constexpr int kBrickH = 9;
    static constexpr int kBallW = 6;  // logical collision size
    static constexpr int kBallH = 6;
    struct Ball
    {
        float x, y;
        float vx, vy;
        float px, py;
        bool active;
    bool isMurder;
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
    // Falling hazard (Destroy Bat Bricks: F1/F2). If it hits the bat, lose a life immediately.
    struct FallingHazard
    {
        float x, y;   // top-left
        float vy;     // vertical velocity
        int type;     // 1 = F1 (slow), 2 = F2 (fast)
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

    // Map per-level speed (1..99, with 10 as default) to a velocity multiplier.
    // This scales the initial launch speed of the ball before any brick-induced modifiers.
    static float level_speed_multiplier()
    {
        int s = levels_get_speed(levels_current());
        if (s <= 0) s = 10; // fallback default
        return (float)s / 10.0f;
    }

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
    std::vector<FallingHazard> hazards; // falling kill bricks (F1/F2)
        C2D_Image imgBall{};
    C2D_Image imgMurderBall{};
    C2D_Image imgBatNormal{};
    C2D_Image imgBatSmall{};
    C2D_Image imgBatBig{};
    int batSizeMode = 1;       // 0=small,1=normal,2=big
    float batCollWidth = (float)BATWIDTH; // logical collision width (changes with bat size)
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
    int murderTimer = 0;    // deprecated: per-ball isMurder now controls behaviour
    // Life-loss death sequence (bat sinks then fade out/in)
    bool deathActive = false;
    int  deathPhase = 0;       // 0=sink, 1=fadeOut, 2=fadeIn
    int  deathTimer = 0;       // generic per-phase timer
    float deathSinkVy = 0.0f;  // current vertical speed of bat sink
    int  deathFadeAlpha = 0;   // overlay alpha while fading (0..200)
    // Laser system (re-implemented as pickup + indicator)
    bool laserEnabled = false; // collected laser ability
    bool laserReady = false;   // indicator visible and ready to fire
    int fireCooldown = 0;      // small debounce (optional); not used for charges
        // Moving brick dynamic traversal across contiguous empty span
        std::vector<MovingBrickData> moving; // per-cell data (pos<0 => unused)
        // Particles (bomb / generic)
        std::vector<Particle> particles;
        std::vector<BombEvent> bombEvents; // pending delayed explosions
    // Barrier hit glow (temporary highlight on top edge when a life is consumed by the barrier)
    int barrierGlowTimer = 0;      // frames remaining for glow (0=off)
    // Game Over sequence (fade to message on top screen)
    bool gameOverActive = false;
    int  gameOverPhase = 0;   // 0=fadeIn, 1=hold, (future: 2=out)
    int  gameOverTimer = 0;   // per-phase timer
    int  gameOverAlpha = 0;   // overlay alpha (0..200)
    };


    static State G;
    static bool g_exitRequested = false;

    // Forward declarations for Game Over sequence helpers
    static void update_game_over();
    static void finalize_game_over();
    void begin_game_over();

        // Simple dust effect: spawn a burst of particles at (x, y)
        void spawn_dust_effect(float x, float y) {
            for (int i = 0; i < 12; ++i) {
                float angle = (float)i / 12.0f * 6.28318f;
                float speed = 0.4f + 0.2f * (i % 3); // smaller spread, half brick size
                uint32_t dustColor = C2D_Color32(245, 245, 245, 220); // nearly white, high opacity
                float px = x;
                float py = y;
                Particle p{ px, py, std::cos(angle) * speed, std::sin(angle) * speed, 10, dustColor };
                G.particles.push_back(p);
            }
        }
    bool exit_requested_internal() { return g_exitRequested; }

    // Forward declare helpers defined later
    static void set_bat_size(int mode);

    void init_assets()
    {
        G.imgBall = hw_image(IMAGE_ball_sprite_idx);
    G.imgMurderBall = hw_image(IMAGE_murderball_sprite_idx);
        G.imgBatNormal = hw_image(IMAGE_bat_normal_idx);
    G.imgBatSmall = hw_image(IMAGE_bat_small_idx);
    G.imgBatBig   = hw_image(IMAGE_bat_big_idx);
    float bw = (G.imgBatNormal.subtex) ? G.imgBatNormal.subtex->width : 64.f;
        float bh = (G.imgBatNormal.subtex) ? G.imgBatNormal.subtex->height : 8.f;
    float batCenterX = kScreenWidth * 0.5f;
        G.bat = {batCenterX - bw / 2.f, kInitialBatY, bw, bh, G.imgBatNormal};
    G.batSizeMode = 1;
            // Set initial bat collider width equal to sprite width (21/32/44 per INF)
            {
                float bw = (G.imgBatNormal.subtex ? G.imgBatNormal.subtex->width : 64.f);
                G.batCollWidth = std::max(8.0f, bw);
            }
    float ballStartX = kScreenWidth * 0.5f - kInitialBallHalf;
    G.balls.push_back({ballStartX, kInitialBallY, 0.0f, 0.f, ballStartX, kInitialBallY, true, false, G.imgBall});
    G.ballLocked = true;
        char buf[96];
        snprintf(buf, sizeof buf, "bat sprite w=%.1f h=%.1f\n", bw, bh);
        hw_log(buf);
    }

    // Begin the life-loss sequence (non-gameover): bat sinks, fade out/in, respawn with parked ball
    static void begin_death_sequence()
    {
        // Decrement life and check for game over
        G.lives--;
        if (G.lives <= 0)
        {
            // If we are in an editor test run, return to editor instead of title
            if (editor::test_return_active()) {
                levels_reset_level(editor::current_level_index());
                editor::on_return_from_test_full();
                G.mode = Mode::Editor;
                // Clear transient states
                G.deathActive = false; G.deathFadeAlpha = 0; G.deathTimer = 0; G.deathPhase = 0; G.deathSinkVy = 0.f;
                G.hazards.clear();
                G.letters.clear();
                return;
            }
            // Begin game-over sequence (fade and message with sound)
            begin_game_over();
            return;
        }
        // Start non-gameover death sequence
        G.deathActive = true;
        G.deathPhase = 0;
        G.deathTimer = 0;
        G.deathSinkVy = 1.2f;
        // Freeze gameplay objects (deactivate balls so only bat is seen sinking)
        for (auto &b : G.balls) b.active = false;
        // Lose laser immediately on life loss
        G.laserEnabled = false;
        G.laserReady = false;
    }

    static void update_death_sequence()
    {
        if (!G.deathActive) return;
        switch (G.deathPhase)
        {
        case 0: // sinking bat
            G.bat.y += G.deathSinkVy;
            G.deathSinkVy += 0.15f; // accelerate
            if (G.bat.y > 500.0f)
            {
                G.deathPhase = 1;
                G.deathTimer = 0;
                G.deathFadeAlpha = 0;
            }
            break;
        case 1: // fade out
            if (G.deathFadeAlpha < 200) G.deathFadeAlpha += 12; // 0.2s-ish
            if (++G.deathTimer > 20)
            {
                // Respawn during black
                // Reset bat position (keep current X clamped) and ball parked
                if (G.bat.x < kPlayfieldLeftWallX) G.bat.x = kPlayfieldLeftWallX;
                if (G.bat.x > kPlayfieldRightWallX - G.bat.width) G.bat.x = kPlayfieldRightWallX - G.bat.width;
                G.bat.y = kInitialBatY;
                // Primary ball parked and relocked
                if (G.balls.empty())
                {
                    float ballStartX = kScreenWidth * 0.5f + kPlayfieldOffsetX - kInitialBallHalf;
                    G.balls.push_back({ballStartX, kInitialBallY, 0.0f, 0.f, ballStartX, kInitialBallY, true, false, G.imgBall});
                }
                Ball &b0 = G.balls[0];
                b0.x = G.bat.x + G.bat.width * 0.5f - kBallW * 0.5f;
                b0.y = G.bat.y - kBallH - 1;
                b0.px = b0.x; b0.py = b0.y; b0.vx = 0.f; b0.vy = 0.f; b0.active = true;
                // Clear any leftover hazards/pickups
                G.hazards.clear();
                G.letters.erase(std::remove_if(G.letters.begin(), G.letters.end(), [](const FallingLetter &f){ return !f.active; }), G.letters.end());
                G.ballLocked = true;
                G.deathPhase = 2;
                G.deathTimer = 0;
            }
            break;
        case 2: // fade in
            if (G.deathFadeAlpha > 0) G.deathFadeAlpha -= 10;
            if (G.deathFadeAlpha < 0) G.deathFadeAlpha = 0;
            if (++G.deathTimer > 18 && G.deathFadeAlpha == 0)
            {
                // Resume normal play
                G.deathActive = false;
                G.deathPhase = 0;
                G.deathTimer = 0;
                G.deathSinkVy = 0;
            }
            break;
        }
    }

    // Game Over sequence API
    void begin_game_over()
    {
        if (G.gameOverActive) return;
        hw_log("GAME OVER\n");
        G.gameOverActive = true;
        G.gameOverPhase = 0;
        G.gameOverTimer = 0;
        G.gameOverAlpha = 0;
        // Stop gameplay interactions
        for (auto &b : G.balls) b.active = false;
        G.laserEnabled = false;
        G.laserReady = false;
        // Play the new game-over sound on a free channel (reserve 3)
    sound::play_sfx("game-over", 3, 1.0f, true);
    }

    static void update_game_over()
    {
        if (!G.gameOverActive) return;
        switch (G.gameOverPhase)
        {
        case 0: // fade in overlay to near-black and show message
            if (G.gameOverAlpha < 200) G.gameOverAlpha += 10; // ~0.33s
            if (++G.gameOverTimer > 60) { // after fade-in, move to hold
                G.gameOverPhase = 1;
                G.gameOverTimer = 0;
            }
            break;
        case 1: // hold
            if (++G.gameOverTimer > 150) { // auto-finalize after ~2.5s
                finalize_game_over();
            }
            break;
        }
    }

    static void finalize_game_over()
    {
        // Submit score and reset to title (similar to prior flow)
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
        }
        G.mode = Mode::Title;
        levels_set_current(0);
        levels_reset_level(0);
        G.balls.clear();
        {
            float ballStartX = kScreenWidth * 0.5f + kPlayfieldOffsetX - kInitialBallHalf;
            G.balls.push_back({ballStartX, kInitialBallY, 0.0f, 0.f, ballStartX, kInitialBallY, true, false, G.imgBall});
        }
        G.ballLocked = true;
        G.lives = 3; // reset lives after returning to title
        set_bat_size(1); // reset bat to normal
        G.score = 0;
        G.bonusBits = 0;
        G.reverseTimer = G.lightsOffTimer = G.murderTimer = 0;
        G.fireCooldown = 0;
        G.letters.clear();
        G.hazards.clear();
        // Clear sequences
        G.deathActive = false; G.deathFadeAlpha = 0; G.deathTimer = 0; G.deathPhase = 0; G.deathSinkVy = 0.f;
        G.gameOverActive = false; G.gameOverAlpha = 0; G.gameOverPhase = 0; G.gameOverTimer = 0;
        // Jump to High screen in title sequence if a new score was placed
        // Keep existing behavior: seqPos=1 is High
        // Note: begin_game_over() already logged; no extra log here
    }

    // Barrier helpers: single source of truth for when it draws and when it collides
    static inline bool barrier_visible() {
        // Visible for lives >= 1; hidden only at 0
        return G.lives > 0;
    }
    static inline bool barrier_collides() {
        // Collides (consumes a life and rebounds) whenever lives > 0 and the ball is launched
        return G.lives > 0 && !G.ballLocked;
    }
    // (Removed color helpers; glow is always white now)

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
    G.balls.push_back(Ball{x, y, vx, vy, x, y, true, false, G.imgBall});
    }

    static void spawn_murder_ball(float x, float y, float vx, float vy)
    {
        if (G.balls.size() >= 8)
            return;
    G.balls.push_back(Ball{x, y, vx, vy, x, y, true, true, G.imgMurderBall});
    }

    // Choose an alternate diagonal for a split ball. Guarantees both components non-zero.
    static void choose_split_velocity(const Ball &src, float &outVx, float &outVy)
    {
        // Preserve overall speed magnitude
        float spd = std::sqrt(src.vx * src.vx + src.vy * src.vy);
        if (spd < 0.01f) spd = 1.5f; // fallback
        // If already diagonal, flip the vertical component to choose a different diagonal
        if (std::fabs(src.vx) > 0.01f && std::fabs(src.vy) > 0.01f)
        {
            outVx = src.vx;
            outVy = -src.vy;
            return;
        }
        // If moving purely horizontal or vertical, pick a diagonal based on current signs
        int sx = (src.vx >= 0.0f) ? 1 : -1;
        int sy = (src.vy >= 0.0f) ? 1 : -1;
        if (std::fabs(src.vx) <= 0.01f && std::fabs(src.vy) > 0.01f)
        {
            // Vertical -> choose left or right keeping opposite vertical direction
            float comp = spd * 0.7071f;
            outVx = (sx == 0 ? 1 : sx) * comp; // default to right if 0
            outVy = -sy * comp;
            return;
        }
        if (std::fabs(src.vy) <= 0.01f && std::fabs(src.vx) > 0.01f)
        {
            // Horizontal -> choose up or down keeping horizontal direction
            float comp = spd * 0.7071f;
            outVx = sx * comp;
            outVy = -1 * comp; // prefer upward split
            return;
        }
        // Degenerate: standstill
        outVx = spd * 0.7071f;
        outVy = -spd * 0.7071f;
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

    static void spawn_laser_pickup(float cx, float cy)
    {
        // Use the laser brick visual for the falling pickup
        C2D_Image img = hw_image(IMAGE_laser_brick_idx);
        float w = (img.subtex ? img.subtex->width : 16.0f);
        float h = (img.subtex ? img.subtex->height : 9.0f);
        // Reuse FallingLetter with code 200 for laser pickup
        FallingLetter fl{cx - w * 0.5f, cy - h * 0.5f, 0.6f, 200, true, img};
        G.letters.push_back(fl);
    }

    // Effect pickup codes (avoid clashes with 0..4, 100/101, 200 used elsewhere)
    enum : int {
        PK_LIFE = 300,
        PK_SLOW = 301,
        PK_FAST = 302,
        PK_REWIND = 303,
        PK_REVERSE = 304,
        PK_FORWARD = 305,
        PK_BONUS1000 = 306,
        PK_LIGHTS_OFF = 307,
        PK_LIGHTS_ON = 308,
    };

    static void spawn_effect_pickup(int effectCode, int atlasIdx, float cx, float cy)
    {
        C2D_Image img = hw_image(atlasIdx);
        float w = (img.subtex ? img.subtex->width : 16.0f);
        float h = (img.subtex ? img.subtex->height : 9.0f);
        FallingLetter fl{cx - w * 0.5f, cy - h * 0.5f, 0.6f, effectCode, true, img};
        G.letters.push_back(fl);
    }

    static void spawn_destroy_bat_brick(BrickType bt, float cx, float cy)
    {
        // Use skull brick visual for both F1/F2 for now
        C2D_Image img = hw_image(IMAGE_skull_brick_idx);
        float w = (img.subtex ? img.subtex->width : 16.0f);
        float h = (img.subtex ? img.subtex->height : 9.0f);
        int t = (bt == BrickType::F2) ? 2 : 1; // 1=F1(slow) 2=F2(fast)
        // Ensure F2 starts at exactly 2x the initial speed of F1
        const float baseInitVy = 0.6f; // F1
        float initVy = baseInitVy * (t == 2 ? 2.0f : 1.0f);
        G.hazards.push_back(FallingHazard{cx - w * 0.5f, cy - h * 0.5f, initVy, t, true, img});
    }

    static void spawn_bat_pickup(bool makeBig, float cx, float cy)
    {
        // Spawn a falling pickup using the batsmall/batbig brick visuals
        int atlasIdx = makeBig ? IMAGE_batbig_brick_idx : IMAGE_batsmall_brick_idx;
        C2D_Image img = hw_image(atlasIdx);
        float w = (img.subtex ? img.subtex->width : 16.0f);
        float h = (img.subtex ? img.subtex->height : 9.0f);
        // Reuse FallingLetter with sentinel letter values: 100=small, 101=big
        int code = makeBig ? 101 : 100;
        FallingLetter fl{cx - w * 0.5f, cy - h * 0.5f, 0.6f, code, true, img};
        G.letters.push_back(fl);
    }

    static void set_bat_size(int mode)
    {
    if (mode < 0) mode = 0;
    if (mode > 2) mode = 2;
        if (G.batSizeMode == mode) return;
        G.batSizeMode = mode;
        // Preserve center X while changing sprite and width
        float centerX = G.bat.x + G.bat.width * 0.5f;
        if (mode == 0)
        {
            G.bat.img = G.imgBatSmall;
            float newW = (G.bat.img.subtex ? G.bat.img.subtex->width : G.bat.width);
            G.bat.width = newW;
                G.batCollWidth = std::max(8.0f, newW);
        }
        else if (mode == 2)
        {
            G.bat.img = G.imgBatBig;
            float newW = (G.bat.img.subtex ? G.bat.img.subtex->width : G.bat.width);
            G.bat.width = newW;
                G.batCollWidth = std::max(8.0f, newW);
        }
        else
        {
            G.bat.img = G.imgBatNormal;
            float newW = (G.bat.img.subtex ? G.bat.img.subtex->width : G.bat.width);
            G.bat.width = newW;
                G.batCollWidth = std::max(8.0f, newW);
        }
        // Height from sprite
        G.bat.height = (G.bat.img.subtex ? G.bat.img.subtex->height : G.bat.height);
        G.bat.x = centerX - G.bat.width * 0.5f;
        // Clamp inside playfield
        if (G.bat.x < kPlayfieldLeftWallX) G.bat.x = kPlayfieldLeftWallX;
        if (G.bat.x > kPlayfieldRightWallX - G.bat.width) G.bat.x = kPlayfieldRightWallX - G.bat.width;
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
            spawn_effect_pickup(PK_LIFE, IMAGE_life_brick_idx, cx, cy);
            break;
        case BrickType::SB:
            spawn_effect_pickup(PK_SLOW, IMAGE_slow_brick_idx, cx, cy);
            break;
        case BrickType::FB:
            spawn_effect_pickup(PK_FAST, IMAGE_fast_brick_idx, cx, cy);
            break;
        case BrickType::AB:
        {
            float svx, svy; choose_split_velocity(ball, svx, svy);
            // Spawn a standard extra ball; original continues without additional reflection handling
            spawn_extra_ball(cx, cy, svx, svy);
        }
            break;
        case BrickType::T5:
            G.score += 60;
            break; // per-hit score (placeholder)
        case BrickType::BO:
            G.score += 120;
            break; // base bomb score before chain
        case BrickType::RW:
            spawn_effect_pickup(PK_REWIND, IMAGE_rewind_brick_idx, cx, cy);
            break;
        case BrickType::FO:
            spawn_effect_pickup(PK_FORWARD, IMAGE_forward_brick_idx, cx, cy);
            break;
        case BrickType::RE:
            spawn_effect_pickup(PK_REVERSE, IMAGE_reverse_brick_idx, cx, cy);
            break; // ~10s pickup
        case BrickType::IS:
            // Immediate slow; pass-through handled in collision code (no bounce)
            ball.vx *= (1.0f - layout::SPEED_MODIFIER);
            ball.vy *= (1.0f - layout::SPEED_MODIFIER);
            break;
        case BrickType::IF:
            // Immediate fast; pass-through handled in collision code (no bounce)
            ball.vx *= (1.0f + layout::SPEED_MODIFIER);
            ball.vy *= (1.0f + layout::SPEED_MODIFIER);
            break;
        case BrickType::LA:
            spawn_laser_pickup(cx, cy);
            break;
        case BrickType::MB:
        {
            float svx, svy; choose_split_velocity(ball, svx, svy);
            // Spawn a murder ball variant while original continues on its path
            spawn_murder_ball(cx, cy, svx, svy);
        }
            break;
        case BrickType::OF:
            spawn_effect_pickup(PK_LIGHTS_OFF, IMAGE_offswitch_brick_idx, cx, cy);
            break;
        case BrickType::ON:
            spawn_effect_pickup(PK_LIGHTS_ON, IMAGE_onswitch_brick_idx, cx, cy);
            break;
        case BrickType::BA:
            spawn_effect_pickup(PK_BONUS1000, IMAGE_bonus_brick_idx, cx, cy);
            break;
        case BrickType::BS:
            spawn_bat_pickup(false, cx, cy);
            break;
        case BrickType::BB:
            spawn_bat_pickup(true, cx, cy);
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
    float effBatW = G.batCollWidth;
        float effBatH = (G.bat.img.subtex ? G.bat.img.subtex->height : G.bat.height);
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
            if (L.y > 480.0f) { L.active = false; continue; }
            float lw = (L.img.subtex ? L.img.subtex->width : 10.0f);
            float lh = (L.img.subtex ? L.img.subtex->height : 11.0f);
            float lLeft = L.x, lTop = L.y, lRight = L.x + lw, lBottom = L.y + lh;
            bool overlap = !(lRight <= batLeft || lLeft >= batRight || lBottom <= batTop || lTop >= batBottom);
            if (overlap)
            {
                // Collect this letter
                switch (L.letter)
                {
                case 0: G.score += 100; G.bonusBits |= 0x01; sound::play_sfx("bonus-step", 5, 1.0f, true); break; // B
                case 1: G.score += 100; G.bonusBits |= 0x02; sound::play_sfx("bonus-step", 5, 1.0f, true); break; // O
                case 2: G.score += 100; G.bonusBits |= 0x04; sound::play_sfx("bonus-step", 5, 1.0f, true); break; // N
                case 3: G.score += 100; G.bonusBits |= 0x08; sound::play_sfx("bonus-step", 5, 1.0f, true); break; // U
                case 4: G.score += 100; G.bonusBits |= 0x10; sound::play_sfx("bonus-step", 5, 1.0f, true); break; // S
                case 100: // Bat smaller
                    set_bat_size(G.batSizeMode - 1);
                    // Bad pickup
                    sound::play_sfx("bad", 5, 1.0f, true);
                    break;
                case 101: // Bat bigger
                    set_bat_size(G.batSizeMode + 1);
                    // Good pickup
                    sound::play_sfx("good", 5, 1.0f, true);
                    break;
                case 200: // Laser pickup
                    G.laserEnabled = true;
                    G.laserReady = true;
                    // Good pickup
                    sound::play_sfx("good", 5, 1.0f, true);
                    break;
                case PK_LIFE:
                    if (G.lives < 99) G.lives++;
                    // Good pickup
                    sound::play_sfx("extra-life", 5, 1.0f, true);
                    break;
                case PK_SLOW:
                    for (auto &b : G.balls) { b.vx *= (1.0f - layout::SPEED_MODIFIER); b.vy *= (1.0f - layout::SPEED_MODIFIER); }
                    // Good pickup
                    sound::play_sfx("good", 5, 1.0f, true);
                    break;
                case PK_FAST:
                    for (auto &b : G.balls) { b.vx *= (1.0f + layout::SPEED_MODIFIER); b.vy *= (1.0f + layout::SPEED_MODIFIER); }
                    // Bad pickup
                    sound::play_sfx("bad", 5, 1.0f, true);
                    break;
                case PK_REWIND:
                {
                    int cur = levels_current();
                    if (levels_count() > 0) { cur = (cur - 1 + levels_count()) % levels_count(); levels_set_current(cur); }
                }
                    break;
                case PK_FORWARD:
                {
                    int cur = levels_current();
                    if (levels_count() > 0) { cur = (cur + 1) % levels_count(); levels_set_current(cur); }
                }
                    // Good pickup
                    sound::play_sfx("good", 5, 1.0f, true);
                    break;
                case PK_REVERSE:
                    G.reverseTimer = 600; // ~10s
                    // Bad pickup
                    sound::play_sfx("bad", 5, 1.0f, true);
                    break;
                case PK_BONUS1000:
                    G.score += 1000;
                    // Good pickup
                    sound::play_sfx("good", 5, 1.0f, true);
                    break;
                case PK_LIGHTS_OFF:
                    G.lightsOffTimer = 600;
                    // Bad pickup
                    sound::play_sfx("bad", 5, 1.0f, true);
                    break;
                case PK_LIGHTS_ON:
                    G.lightsOffTimer = 0;
                    // Good pickup
                    sound::play_sfx("good", 5, 1.0f, true);
                    break;
                }
                L.active = false;
                // If all five collected, award 250 * level number and reset
                if (G.bonusBits == 0x1F)
                {
                    int lvl = levels_current();
                    int levelNumber = lvl + 1; // levels are 1-based for scoring
                    G.score += 250 * levelNumber;
                    G.bonusBits = 0;
                    sound::play_sfx("all-bonus", 5, 1.0f, true);
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

    static void update_falling_hazards()
    {
        if (G.hazards.empty()) return;
    // Compute effective bat collision rectangle (same logic as pickups)
    float effBatW = G.batCollWidth;
        float effBatH = (G.bat.img.subtex ? G.bat.img.subtex->height : G.bat.height);
        float atlasLeft = (G.bat.img.subtex ? G.bat.img.subtex->left : 0.0f);
        float batPadX = (G.bat.width - effBatW) * 0.5f; if (batPadX < 0) batPadX = 0;
        float batPadY = (G.bat.height - effBatH) * 0.5f; if (batPadY < 0) batPadY = 0;
        float batLeft = G.bat.x + batPadX - atlasLeft;
        float batTop = G.bat.y + batPadY;
        float batRight = batLeft + effBatW;
        float batBottom = batTop + effBatH;
        for (auto &H : G.hazards)
        {
            if (!H.active) continue;
            H.y += H.vy;
            // Apply gravity; F2 accelerates at 2x so it maintains ~2x speed profile
            const float baseGrav = 0.025f; // F1 gravity per frame
            H.vy += baseGrav * (H.type == 2 ? 2.0f : 1.0f);
            if (H.y > 480.0f) { H.active = false; continue; }
            float hw = (H.img.subtex ? H.img.subtex->width : 16.0f);
            float hh = (H.img.subtex ? H.img.subtex->height : 9.0f);
            float hLeft = H.x, hTop = H.y, hRight = H.x + hw, hBottom = H.y + hh;
            bool overlap = !(hRight <= batLeft || hLeft >= batRight || hBottom <= batTop || hTop >= batBottom);
            if (overlap)
            {
                // Play hazard pickup SFX; avoid double-playing if this will cause Game Over
                if (G.lives > 1)
                    sound::play_sfx("game-over", 3, 1.0f, true);
                H.active = false;
                begin_death_sequence();
                return; // bail; sequence takes control
            }
        }
        // Compact occasionally
        if ((int)G.hazards.size() > 32)
        {
            G.hazards.erase(std::remove_if(G.hazards.begin(), G.hazards.end(), [](const FallingHazard &h){ return !h.active; }), G.hazards.end());
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
    static void update_falling_hazards(); // fwd

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
            int cellW = levels_brick_width();
            int cellH = levels_brick_height();
            // Align logical 6x6 collider centered within the rendered sprite (handles atlas trims)
            float spriteW = (ball.img.subtex ? ball.img.subtex->width : (float)kBallW);
            float spriteH = (ball.img.subtex ? ball.img.subtex->height : (float)kBallH);
            float ballL = ball.x + (spriteW - (float)kBallW) * 0.5f;
            float ballT = ball.y + (spriteH - (float)kBallH) * 0.5f;
            float ballR = ballL + (float)kBallW;
            float ballB = ballT + (float)kBallH;
            // center values can be derived on demand; avoid unused locals

            if (!movingPhase)
            {
                int minCol = (int)((ballL - ls) / cellW);
                if (minCol < 0)
                    minCol = 0;
                if (minCol >= kBrickCols)
                    return false;
                int maxCol = (int)((ballR - 1 - ls) / cellW);
                if (maxCol < 0)
                    return false;
                if (maxCol >= kBrickCols)
                    maxCol = kBrickCols - 1;
                int minRow = (int)((ballT - ts) / cellH);
                if (minRow < 0)
                    minRow = 0;
                if (minRow >= kBrickRows)
                    return false;
                int maxRow = (int)((ballB - 1 - ts) / cellH);
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
                        float bx = ls + c * cellW;
                        float by = ts + r * cellH;
                        float br = bx + cellW;
                        float bb = by + cellH;
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
                            apply_brick_effect(BrickType::BO, bx + cellW * 0.5f, by + cellH * 0.5f, ball);
                            for (int k = 0; k < 8; k++)
                            {
                                float angle = (float)k / 8.f * 6.28318f;
                                float sp = 0.6f + 0.4f * (k % 4);
                                Particle p{bx + cellW * 0.5f, by + cellH * 0.5f, std::cos(angle) * sp, std::sin(angle) * sp, 32, C2D_Color32(255, 200, 50, 255)};
                                G.particles.push_back(p);
                            }
                            schedule_neighbor_bombs(c, r, 15);
                        }
                        else if (bt == BrickType::ID)
                            destroyed = false;
                        else if (bt == BrickType::F1 || bt == BrickType::F2)
                        {
                            levels_remove_brick(c, r);
                            apply_brick_effect(bt, bx + cellW * 0.5f, by + cellH * 0.5f, ball);
                            spawn_destroy_bat_brick(bt, bx + cellW * 0.5f, by + cellH * 0.5f);
                        }
                        else
                            levels_remove_brick(c, r);
                        apply_brick_effect(bt, bx + cellW * 0.5f, by + cellH * 0.5f, ball);
                        // Play context-aware SFX: ID/SF and T5 non-final hits use a harder impact
                        {
                            // Use channel 6 for T5 final break to avoid debounce suppression on channel 1
                            if (bt == BrickType::T5 && destroyed) {
                                // Ensure prior hit-hard on brick channel doesn't mask the final break
                                sound::stop_sfx_channel(1);
                                sound::play_sfx("hard-explode", 6, 1.0f, true);
                            } else {
                                const char* sfx = "ball-brick";
                                if (bt == BrickType::ID || bt == BrickType::SF) sfx = "hit-hard";
                                else if (bt == BrickType::T5) sfx = "hit-hard"; // non-final hits
                                sound::play_sfx(sfx, 1, 1.0f, true);
                            }
                        }
                        // For AB/MB bricks, original ball continues without reflecting this frame
                        if (bt == BrickType::AB || bt == BrickType::MB)
                        {
                            if (destroyed && levels_remaining_breakable() == 0 && levels_count() > 0) {
                                if(!editor::test_return_active()) {
                                    int next = (levels_current() + 1) % levels_count();
                                    levels_set_current(next);
                                    set_bat_size(1); // reset to normal at level end
                                    G.laserEnabled = false;
                                    G.laserReady = false;
                                    hw_log("LEVEL COMPLETE\n");
                                }
                            }
                            return true;
                        }
                        if (!ball.isMurder)
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
                            if (bt != BrickType::AB && bt != BrickType::MB && bt != BrickType::IS && bt != BrickType::IF)
                            {
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
                        }
            if (destroyed && levels_remaining_breakable() == 0 && levels_count() > 0) {
                            if(!editor::test_return_active()) {
                                int next = (levels_current() + 1) % levels_count();
                                levels_set_current(next);
                set_bat_size(1); // reset to normal at level end
                                G.laserEnabled = false;
                                G.laserReady = false;
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
                int cellW = levels_brick_width();
                int cellH = levels_brick_height();
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
                        float by = ts2 + r * cellH;
                        float br = bx + cellW;
                        float bb = by + cellH;
                        if (ballR <= bx || ballL >= br || ballB <= by || ballT >= bb)
                            continue;
                        BrickType bt = (BrickType)raw;
                        bool destroyed = true;
                        if (bt == BrickType::T5)
                            destroyed = levels_damage_brick(c, r);
                        else if (bt == BrickType::BO)
                        {
                            levels_remove_brick(c, r);
                            apply_brick_effect(BrickType::BO, bx + cellW * 0.5f, by + cellH * 0.5f, ball);
                            for (int k = 0; k < 8; k++)
                            {
                                float angle = (float)k / 8.f * 6.28318f;
                                float sp = 0.6f + 0.4f * (k % 4);
                                Particle p{bx + cellW * 0.5f, by + cellH * 0.5f, std::cos(angle) * sp, std::sin(angle) * sp, 32, C2D_Color32(255, 200, 50, 255)};
                                G.particles.push_back(p);
                            }
                            schedule_neighbor_bombs(c, r, 15);
                        }
                        else if (bt == BrickType::ID)
                            destroyed = false;
                        else if (bt == BrickType::F1 || bt == BrickType::F2)
                        {
                            levels_remove_brick(c, r);
                            apply_brick_effect(bt, bx + cellW * 0.5f, by + cellH * 0.5f, ball);
                            spawn_destroy_bat_brick(bt, bx + cellW * 0.5f, by + cellH * 0.5f);
                        }
                        else
                            levels_remove_brick(c, r);
                        apply_brick_effect(bt, bx + cellW * 0.5f, by + cellH * 0.5f, ball);
                        // Play context-aware SFX for moving bricks as well
                        {
                            if (bt == BrickType::T5 && destroyed) {
                                sound::stop_sfx_channel(1);
                                sound::play_sfx("hard-explode", 6, 1.0f, true);
                            } else {
                                const char* sfx = "ball-brick";
                                if (bt == BrickType::ID || bt == BrickType::SF) sfx = "hit-hard";
                                else if (bt == BrickType::T5) sfx = "hit-hard";
                                sound::play_sfx(sfx, 1, 1.0f, true);
                            }
                        }
                        if (bt == BrickType::AB || bt == BrickType::MB)
                        {
                            if (destroyed && levels_remaining_breakable() == 0 && levels_count() > 0) {
                                if(!editor::test_return_active()) {
                                    int next = (levels_current() + 1) % levels_count();
                                    levels_set_current(next);
                                    set_bat_size(1); // reset to normal at level end
                                    hw_log("LEVEL COMPLETE\n");
                                }
                            }
                            return true;
                        }
                        if (!ball.isMurder)
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
                            if (bt != BrickType::AB && bt != BrickType::MB && bt != BrickType::IS && bt != BrickType::IF)
                            {
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
                        }
            if (destroyed && levels_remaining_breakable() == 0 && levels_count() > 0) {
                            if(!editor::test_return_active()) {
                                int next = (levels_current() + 1) % levels_count();
                                levels_set_current(next);
                set_bat_size(1); // reset to normal at level end
                                hw_log("LEVEL COMPLETE\n");
                            }
                        }
                        return true;
                    }
                return false;
            }
        };

    // Sub-step integration to prevent tunnelling, especially through indestructibles at high speeds.
    // We subdivide the frame movement so that translation per substep is <= 0.5px on the fastest axis. On first
        // collision we stop (maintaining one-hit-per-frame behaviour).
    const float kSubstepPixels = 0.5f; // higher resolution to reduce miss chance on thin seams
    float maxAxis = std::max(std::fabs(ball.vx), std::fabs(ball.vy));
    int steps = (int)std::ceil(maxAxis / kSubstepPixels);
    if (steps > 64) steps = 64; // safety cap
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
        ball.x = startX;
        ball.y = startY;
        float remainingT = 1.0f;
        for (int substep = 0; substep < steps && remainingT > 0.0f; ++substep) {
            float stepT = remainingT / (steps - substep);
            float endX = ball.x + totalDX * stepT;
            float endY = ball.y + totalDY * stepT;
            // Swept AABB collision detection
            float earliestT = stepT;
            int hitC = -1, hitR = -1;
            float hitPenX = 0, hitPenY = 0;
            int ls = levels_left();
            int ts = levels_top();
            int cellW = levels_brick_width();
            int cellH = levels_brick_height();
            float spriteW = (ball.img.subtex ? ball.img.subtex->width : (float)kBallW);
            float spriteH = (ball.img.subtex ? ball.img.subtex->height : (float)kBallH);
            float ballL0 = ball.x + (spriteW - (float)kBallW) * 0.5f;
            float ballT0 = ball.y + (spriteH - (float)kBallH) * 0.5f;
            float ballR0 = ballL0 + (float)kBallW;
            float ballB0 = ballT0 + (float)kBallH;
            float ballL1 = endX + (spriteW - (float)kBallW) * 0.5f;
            float ballT1 = endY + (spriteH - (float)kBallH) * 0.5f;
            float ballR1 = ballL1 + (float)kBallW;
            float ballB1 = ballT1 + (float)kBallH;
            int minCol = (int)((std::min(ballL0, ballL1) - ls) / cellW);
            int maxCol = (int)(((std::max(ballR0, ballR1) - ls) - 0.001f) / cellW);
            int minRow = (int)((std::min(ballT0, ballT1) - ts) / cellH);
            int maxRow = (int)(((std::max(ballB0, ballB1) - ts) - 0.001f) / cellH);
            if (minCol < 0) minCol = 0;
            if (maxCol >= kBrickCols) maxCol = kBrickCols - 1;
            if (minRow < 0) minRow = 0;
            if (maxRow >= kBrickRows) maxRow = kBrickRows - 1;
            for (int r = minRow; r <= maxRow; ++r) {
                for (int c = minCol; c <= maxCol; ++c) {
                    int raw = levels_brick_at(c, r);
                    if (raw <= 0 || is_moving_type(raw)) continue;
                    float bx = ls + c * cellW;
                    float by = ts + r * cellH;
                    float br = bx + cellW;
                    float bb = by + cellH;
                    // Swept AABB: calculate time of impact
                    float tEntryX, tExitX, tEntryY, tExitY;
                    float vx = endX - ball.x;
                    float vy = endY - ball.y;
                    if (vx > 0) {
                        tEntryX = (bx - ballR0) / vx;
                        tExitX = (br - ballL0) / vx;
                    } else if (vx < 0) {
                        tEntryX = (br - ballL0) / vx;
                        tExitX = (bx - ballR0) / vx;
                    } else {
                        tEntryX = -INFINITY;
                        tExitX = INFINITY;
                    }
                    if (vy > 0) {
                        tEntryY = (by - ballB0) / vy;
                        tExitY = (bb - ballT0) / vy;
                    } else if (vy < 0) {
                        tEntryY = (bb - ballT0) / vy;
                        tExitY = (by - ballB0) / vy;
                    } else {
                        tEntryY = -INFINITY;
                        tExitY = INFINITY;
                    }
                    float tEntry = std::max(tEntryX, tEntryY);
                    float tExit = std::min(tExitX, tExitY);
                    if (tEntry < 0.0f || tEntry > stepT || tEntry > tExit || tExit < 0.0f || tEntry > earliestT)
                        continue;
                    // Found earlier collision
                    earliestT = tEntry;
                    hitC = c;
                    hitR = r;
                    hitPenX = (tEntryX > tEntryY) ? 1 : 0;
                    hitPenY = (tEntryY >= tEntryX) ? 1 : 0;
                }
            }
            if (hitC != -1 && hitR != -1) {
                // Move ball to collision point
                ball.x += totalDX * earliestT;
                ball.y += totalDY * earliestT;
                // Resolve collision
                int raw = levels_brick_at(hitC, hitR);
                BrickType bt = (BrickType)raw;
                float bx = ls + hitC * cellW;
                float by = ts + hitR * cellH;
                // For AB/MB we split a new ball and let the original continue without reflection.
                if (bt != BrickType::AB && bt != BrickType::MB && bt != BrickType::IS && bt != BrickType::IF) {
                    if (hitPenX)
                        ball.vx = -ball.vx;
                    if (hitPenY)
                        ball.vy = -ball.vy;
                }
                bool destroyed = true;
                if (bt == BrickType::T5)
                    destroyed = levels_damage_brick(hitC, hitR);
                else if (bt == BrickType::BO) {
                    levels_remove_brick(hitC, hitR);
                    apply_brick_effect(BrickType::BO, bx + cellW * 0.5f, by + cellH * 0.5f, ball);
                    for (int k = 0; k < 8; k++) {
                        float angle = (float)k / 8.f * 6.28318f;
                        float sp = 0.6f + 0.4f * (k % 4);
                        Particle p{bx + cellW * 0.5f, by + cellH * 0.5f, std::cos(angle) * sp, std::sin(angle) * sp, 32, C2D_Color32(255, 200, 50, 255)};
                        G.particles.push_back(p);
                    }
                    schedule_neighbor_bombs(hitC, hitR, 15);
                } else if (bt == BrickType::ID) {
                    destroyed = false;
                } else if (bt == BrickType::F1 || bt == BrickType::F2) {
                    levels_remove_brick(hitC, hitR);
                    apply_brick_effect(bt, bx + cellW * 0.5f, by + cellH * 0.5f, ball);
                    spawn_destroy_bat_brick(bt, bx + cellW * 0.5f, by + cellH * 0.5f);
                } else {
                    levels_remove_brick(hitC, hitR);
                }
                apply_brick_effect(bt, bx + cellW * 0.5f, by + cellH * 0.5f, ball);
                // Play context-aware SFX for swept collision
                {
                    if (bt == BrickType::T5 && destroyed) {
                        sound::stop_sfx_channel(1);
                        sound::play_sfx("hard-explode", 6, 1.0f, true);
                    } else {
                        const char* sfx = "ball-brick";
                        if (bt == BrickType::ID || bt == BrickType::SF) sfx = "hit-hard";
                        else if (bt == BrickType::T5) sfx = "hit-hard";
                        sound::play_sfx(sfx, 1, 1.0f, true);
                    }
                }
                if (bt == BrickType::AB || bt == BrickType::MB) {
                    remainingT -= earliestT;
                    break;
                }
                // Only one hit per frame
                if (destroyed && levels_remaining_breakable() == 0 && levels_count() > 0) {
                    if(!editor::test_return_active()) {
                        int next = (levels_current() + 1) % levels_count();
                        levels_set_current(next);
                        set_bat_size(1);
                        hw_log("LEVEL COMPLETE\n");
                    }
                }
                remainingT -= earliestT;
                break;
            } else {
                // No collision, move to end of substep
                ball.x = endX;
                ball.y = endY;
                remainingT -= stepT;
            }
        }
        // Final guard: if we end the frame embedded in any solid brick, resolve now.
        // This covers edge cases where the swept test missed due to starting overlapped at a substep.
        {
            int ls = levels_left();
            int ts = levels_top();
            int cw = levels_brick_width();
            int ch = levels_brick_height();
            float spriteW = (ball.img.subtex ? ball.img.subtex->width : (float)kBallW);
            float spriteH = (ball.img.subtex ? ball.img.subtex->height : (float)kBallH);
            float ballL = ball.x + (spriteW - (float)kBallW) * 0.5f;
            float ballT = ball.y + (spriteH - (float)kBallH) * 0.5f;
            float ballR = ballL + (float)kBallW;
            float ballB = ballT + (float)kBallH;
            int minCol = (int)((ballL - ls) / cw);
            int maxCol = (int)((ballR - 1 - ls) / cw);
            int minRow = (int)((ballT - ts) / ch);
            int maxRow = (int)((ballB - 1 - ts) / ch);
            if (minCol < 0) minCol = 0;
            if (maxCol >= kBrickCols) maxCol = kBrickCols - 1;
            if (minRow < 0) minRow = 0;
            if (maxRow >= kBrickRows) maxRow = kBrickRows - 1;
            for (int r = minRow; r <= maxRow; ++r)
                for (int c = minCol; c <= maxCol; ++c)
                {
                    int raw = levels_brick_at(c, r);
                    if (raw <= 0 || is_moving_type(raw)) continue;
                    float bx = ls + c * cw, by = ts + r * ch;
                    float br = bx + cw, bb = by + ch;
                    if (ballR <= bx || ballL >= br || ballB <= by || ballT >= bb) continue;

                    // Handle destruction/damage/effects similar to main collision path
                    BrickType bt = (BrickType)raw;
                    bool destroyed = true;
                    if (bt == BrickType::T5) {
                        destroyed = levels_damage_brick(c, r);
                    } else if (bt == BrickType::BO) {
                        levels_remove_brick(c, r);
                        apply_brick_effect(BrickType::BO, bx + cw * 0.5f, by + ch * 0.5f, ball);
                        for (int k = 0; k < 8; k++) {
                            float angle = (float)k / 8.f * 6.28318f;
                            float sp = 0.6f + 0.4f * (k % 4);
                            Particle p{bx + cw * 0.5f, by + ch * 0.5f, std::cos(angle) * sp, std::sin(angle) * sp, 32, C2D_Color32(255, 200, 50, 255)};
                            G.particles.push_back(p);
                        }
                        schedule_neighbor_bombs(c, r, 15);
                    } else if (bt == BrickType::ID) {
                        destroyed = false;
                    } else if (bt == BrickType::F1 || bt == BrickType::F2) {
                        levels_remove_brick(c, r);
                        apply_brick_effect(bt, bx + cw * 0.5f, by + ch * 0.5f, ball);
                        spawn_destroy_bat_brick(bt, bx + cw * 0.5f, by + ch * 0.5f);
                    } else {
                        levels_remove_brick(c, r);
                    }
                    apply_brick_effect(bt, bx + cw * 0.5f, by + ch * 0.5f, ball);
                    // Play context-aware SFX for embedded fix-up
                    {
                        if (bt == BrickType::T5 && destroyed) {
                            sound::stop_sfx_channel(1);
                            sound::play_sfx("hard-explode", 6, 1.0f, true);
                        } else {
                            const char* sfx = "ball-brick";
                            if (bt == BrickType::ID || bt == BrickType::SF) sfx = "hit-hard";
                            else if (bt == BrickType::T5) sfx = "hit-hard";
                            sound::play_sfx(sfx, 1, 1.0f, true);
                        }
                    }

                    // Reflect if not murder ball, and not AB/MB
                    if (bt != BrickType::AB && bt != BrickType::MB && bt != BrickType::IS && bt != BrickType::IF) {
                        float penLeft = ballR - bx;
                        float penRight = br - ballL;
                        float penTop = ballB - by;
                        float penBottom = bb - ballT;
                        float penX = std::min(penLeft, penRight);
                        float penY = std::min(penTop, penBottom);
                        if (penX < penY) {
                            if (penLeft < penRight) ball.x -= penLeft; else ball.x += penRight;
                            ball.vx = -ball.vx;
                        } else {
                            if (penTop < penBottom) ball.y -= penTop; else ball.y += penBottom;
                            ball.vy = -ball.vy;
                        }
                    }

                    if (destroyed && levels_remaining_breakable() == 0 && levels_count() > 0) {
                        if(!editor::test_return_active()) {
                            int next = (levels_current() + 1) % levels_count();
                            levels_set_current(next);
                            set_bat_size(1);
                            hw_log("LEVEL COMPLETE\n");
                        }
                    }
                    return;
                }
        }
    // After swept/static resolution, also check moving bricks once this frame so SS/SF collide properly.
    (void)detect_and_resolve(true);
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
        if (!G.laserEnabled || !G.laserReady || G.fireCooldown > 0)
            return;
        // Only one laser at a time
        for (const auto &L : G.lasers)
            if (L.active)
                return;
        G.fireCooldown = 6; // small debounce so a single press fires once
        G.lasers.push_back({G.bat.x + G.bat.width / 2 - 1, G.bat.y - 6, true});
        G.laserReady = false; // indicator hides while the beam is active
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
                        bool appliedInBranch = false;
                        if (bt == BrickType::T5)
                        {
                            (void)levels_damage_brick(col, row);
                        }
                        else if (bt == BrickType::BO)
                        {
                            std::vector<DestroyedBrick> list;
                            levels_explode_bomb(col, row, &list);
                            for (auto &db : list)
                            {
                                float cx = ls + db.col * cw + cw * 0.5f;
                                float cy = ts + db.row * ch + ch * 0.5f;
                                apply_brick_effect((BrickType)db.type, cx, cy, G.balls[0]);
                                if (db.type == (int)BrickType::F1 || db.type == (int)BrickType::F2)
                                    spawn_destroy_bat_brick((BrickType)db.type, cx, cy);
                            }
                            appliedInBranch = true; // effects already applied for all destroyed bricks
                        }
                        else if (bt == BrickType::ID)
                        { /* indestructible: no action */
                        }
                        else
                        {
                            levels_remove_brick(col, row);
                        }
                        // Apply effect at the center of the hit brick so falling pickups spawn in place
                        if (!appliedInBranch)
                        {
                            float cx = ls + col * cw + cw * 0.5f;
                            float cy = ts + row * ch + ch * 0.5f;
                            apply_brick_effect(bt, cx, cy, G.balls[0]);
                            if (bt == BrickType::F1 || bt == BrickType::F2)
                                spawn_destroy_bat_brick(bt, cx, cy);
                        }
                        L.active = false;
                    }
                }
            }
        // If no active lasers and ability is enabled, show indicator again
        bool anyActive = false;
        for (const auto &L : G.lasers) if (L.active) { anyActive = true; break; }
        if (!anyActive && G.laserEnabled)
            G.laserReady = true;
    }

    // (Options menu state & logic extracted to options.cpp)

    void update(const InputState &in)
    {
        // Keep brick logic in world coordinates; runtime brick Y offset matches layout (no extra padding)
        if (G.mode == Mode::Playing) {
            if (levels_get_draw_offset() != 0) levels_set_draw_offset(0);
            if (levels_get_draw_offset_y() != 0) levels_set_draw_offset_y(0);
        } else if (G.mode == Mode::Editor) {
            if (levels_get_draw_offset() != 0) levels_set_draw_offset(0);
            if (levels_get_draw_offset_y() != 0) levels_set_draw_offset_y(0);
        }
    if (G.mode == Mode::Title)
        {
            // Touch-driven title buttons now trigger on release (press-release inside same button)
            static int sPressedBtn = -1; // index into kTitleButtons while stylus held
            // Physical button mappings: START=Play, SELECT=Editor, X=Exit
            if (in.startPressed)
            {
                sound::play_sfx("menu-click", 4, 1.0f, true);
                levels_set_current(0);
                levels_reset_level(0);
                // Fresh play session: reset full game state to avoid carry-over from editor/test
                G.lives = 3; // ensure new game starts with full lives
                // Reset balls to a single parked ball locked to the bat
                G.balls.clear();
                {
                    float ballStartX = kScreenWidth * 0.5f + kPlayfieldOffsetX - kInitialBallHalf;
                    G.balls.push_back({ballStartX, kInitialBallY, 0.0f, 0.f, ballStartX, kInitialBallY, true, false, G.imgBall});
                }
                G.ballLocked = true;
                set_bat_size(1);
                // Reset score/timers and transient objects
                G.score = 0;
                G.bonusBits = 0;
                G.reverseTimer = G.lightsOffTimer = G.murderTimer = 0;
                G.fireCooldown = 0;
                G.letters.clear();
                G.hazards.clear();
                // Reinitialize moving bricks data for current layout
                {
                    int totalCells = levels_grid_width() * levels_grid_height();
                    G.moving.assign(totalCells, {-1.f, 1.f, 0.f, 0.f});
                }
                G.mode = Mode::Playing;
                hw_log("start (START)\n");
                G.prevTouching = in.touching; // keep touch edge tracking consistent
                G.levelIntroTimer = 90; // show intro ~1.5s
                return;
            }
            if (in.selectPressed)
            {
                sound::play_sfx("menu-click", 4, 1.0f, true);
                G.mode = Mode::Editor;
                hw_log("editor (SELECT)\n");
                G.prevTouching = in.touching;
                return;
            }
            if (in.xPressed)
            {
                sound::play_sfx("menu-click", 4, 1.0f, true);
                g_exitRequested = true;
                hw_log("exit (X)\n");
                G.prevTouching = in.touching;
                return;
            }
            if (in.selectPressed)
            {
                sound::play_sfx("menu-click", 4, 1.0f, true);
                G.mode = Mode::Editor;
                hw_log("editor (SELECT)\n");
                G.prevTouching = in.touching;
                return;
            }
            if (in.xPressed)
            {
                sound::play_sfx("menu-click", 4, 1.0f, true);
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
                    sound::play_sfx("menu-click", 4, 1.0f, true);
                    // We treat release as valid regardless of final coords (optional: require inside)
                    TitleBtn &tb = kTitleButtons[sPressedBtn];
                    if (tb.next == Mode::Playing)
                    {
                        levels_set_current(0);
                        levels_reset_level(0);
                        // Fresh play session: reset full game state to avoid carry-over from editor/test
                        G.lives = 3; // ensure touch-Play resets lives
                        // Reset balls to a single parked ball locked to the bat
                        G.balls.clear();
                        {
                            float ballStartX = kScreenWidth * 0.5f + kPlayfieldOffsetX - kInitialBallHalf;
                            G.balls.push_back({ballStartX, kInitialBallY, 0.0f, 0.f, ballStartX, kInitialBallY, true, false, G.imgBall});
                        }
                        G.ballLocked = true;
                        set_bat_size(1);
                        // Reset score/timers and transient objects
                        G.score = 0;
                        G.bonusBits = 0;
                        G.reverseTimer = G.lightsOffTimer = G.murderTimer = 0;
                        G.fireCooldown = 0;
                        G.letters.clear();
                        G.hazards.clear();
                        // Reinitialize moving bricks data for current layout
                        {
                            int totalCells = levels_grid_width() * levels_grid_height();
                            G.moving.assign(totalCells, {-1.f, 1.f, 0.f, 0.f});
                        }
                        G.levelIntroTimer = 90;
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
    // Update Game Over sequence regardless of mode, but it only activates from Playing
    update_game_over();

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
                    G.balls.push_back({ballStartX, kInitialBallY, 0.0f, 0.f, ballStartX, kInitialBallY, true, false, G.imgBall});
                }
                G.ballLocked = true;
                G.lives = 3; // ensure lives reset (updated default)
                set_bat_size(1);
                G.score = 0;
                G.bonusBits = 0;
                G.reverseTimer = G.lightsOffTimer = G.murderTimer = 0;
                G.fireCooldown = 0;
                G.letters.clear();
                G.hazards.clear();
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
        // If Game Over active, allow finalize via A/Start after hold but continue to render overlay later
        if (G.gameOverActive && G.gameOverPhase >= 1 && (in.aPressed || in.startPressed)) {
            finalize_game_over();
        }
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
                    G.balls.push_back(Ball{ballStartX, kInitialBallY, 0.0f, 0.f, ballStartX, kInitialBallY, true, false, G.imgBall});
                }
                G.ballLocked = true;
                G.lives = 3; // reset lives when starting from Title
                G.score = 0;
                G.bonusBits = 0;
                G.reverseTimer = G.lightsOffTimer = G.murderTimer = 0;
                G.fireCooldown = 0;
                G.letters.clear();
                G.hazards.clear();
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
        // Stylus drag controls bat X (relative). Disabled during death sequence.
        if (!G.deathActive) {
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
        }
        // Fire laser on input edge: D-Pad Up, or stylus release (disabled during death sequence)
        if (!G.deathActive && (in.dpadUpPressed || (G.prevTouching && !in.touching)))
            fire_laser();
    if (!G.deathActive && !G.gameOverActive) update_lasers();
    if (G.reverseTimer > 0)
            --G.reverseTimer;
        if (G.lightsOffTimer > 0)
            --G.lightsOffTimer;
    // per-ball murder behavior; no global timer countdown needed
    if (!G.deathActive && !G.gameOverActive) update_moving_bricks();
    if (!G.deathActive && !G.gameOverActive) update_bonus_letters();
    if (!G.deathActive && !G.gameOverActive) update_falling_hazards();
    update_death_sequence();
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
                    // Wall bounce SFX uses the same brick-hit sound (channel 1)
                    sound::play_sfx("ball-brick", 1, 1.0f, true);
                }
                if (b.x > kPlayfieldRightWallX - kBallW)
                {
                    b.x = kPlayfieldRightWallX - kBallW;
                    b.vx = -b.vx;
                    // Wall bounce SFX
                    sound::play_sfx("ball-brick", 1, 1.0f, true);
                }
                if (b.y < kPlayfieldTopWallY)
                {
                    b.y = kPlayfieldTopWallY;
                    b.vy = -b.vy;
                    // Ceiling bounce SFX
                    sound::play_sfx("ball-brick", 1, 1.0f, true);
                }
                // Barrier life: if there's exactly one regular ball and lives>0, bounce off a barrier below the bat and lose a life
                {
                    // Count active regular (non-murder) balls
                    int regActive = 0;
                    for (const auto &bb : G.balls) if (bb.active && !bb.isMurder) ++regActive;
                    // Unified barrier collision condition
                    if (barrier_collides() && regActive == 1 && !b.isMurder && b.vy > 0)
                    {
                        // Compute barrier top Y using layout-configured offset
                        float effBatH = (G.bat.img.subtex ? G.bat.img.subtex->height : G.bat.height);
                        float barrierTopY = G.bat.y + effBatH + layout::BARRIER_OFFSET_BELOW_BAT;
                        // Compute logical ball bottom previous and current positions (account for sprite vs collider size)
                        float spriteH = (b.img.subtex ? b.img.subtex->height : (float)kBallH);
                        float ballTopPrev = b.py + (spriteH - (float)kBallH) * 0.5f;
                        float ballBottomPrev = ballTopPrev + (float)kBallH;
                        float ballTop = b.y + (spriteH - (float)kBallH) * 0.5f;
                        float ballBottom = ballTop + (float)kBallH;
                        bool crossedBarrier = (ballBottomPrev <= barrierTopY && ballBottom >= barrierTopY);
                        if (crossedBarrier)
                        {
                            if (G.lives > 0) {
                                // Consume life first so tint matches the new barrier state (green/orange/red)
                                G.lives--;
                                // Play barrier hit SFX (channel 2 reserved for barrier events)
                                sound::play_sfx("barrier-hit", 2, 1.0f, true);
                                // Trigger white glow for a short duration
                                G.barrierGlowTimer = kBarrierGlowFrames;
                            }
                            // Reflect the ball off the barrier and place it just above the barrier
                            float adjust = (ballBottom - barrierTopY);
                            b.y -= adjust;
                            b.vy = -b.vy;
                            // Do not process bottom loss this frame
                            continue;
                        }
                    }
                }
                // bottom: lose life (only applies when no barrier is active or ball skipped it)
                if (b.y > 480)
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
                        // Begin the new game-over sequence instead of immediate reset
                        begin_game_over();
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
                    // Lose laser on life loss
                    G.laserEnabled = false;
                    G.laserReady = false;
                    continue;
                }
                // Bat collision using updated logical sizes (bat width follows sprite; ball 6x6)
                if (b.vy > 0)
                {
                    constexpr float ballCollW = (float)kBallW;  // logical collision width
                    constexpr float ballCollH = (float)kBallH;  // logical collision height
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
                    // Bat effective rectangle: centered reduced width (batCollWidth) and legacy height
                    float effBatW = G.batCollWidth;
                    float effBatH = (G.bat.img.subtex ? G.bat.img.subtex->height : G.bat.height);
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
                        if (b.isMurder) {
                            // Murderball kills on bat hit
                            begin_death_sequence();
                            b.active = false; // remove this ball; death sequence takes over
                            continue;
                        }
                        // Normal ball hits the bat: play SFX (exclude Murderball)
                        sound::play_sfx("ball-bat", 0, 1.0f, true);
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

    // Manual launch: require a touch THEN release after lock (disabled during death sequence)
    if (G.ballLocked && !G.deathActive)
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
                    // Apply per-level speed to initial launch velocity (both axes)
                    float mul = level_speed_multiplier();
                    b0.vx *= mul;
                    b0.vy *= mul;
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
    // Ensure correct brick horizontal offset per mode (no background-aligned shift anymore)
    int desiredOffset = 0; // gameplay and editor both use base left now
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
        // NOTE: Backgrounds removed for new dual-screen refactor; gameplay renders on plain backdrop.
    // (Previously drew BREAK.png here.)
        // Top-screen phase (HUD and brick field). Gameplay objects are drawn on both screens appropriately.
    if (G.mode == Mode::Playing) {
            hw_set_top();
        // Draw background (top screen half aligned to screen top, under the HUD)
            {
                C2D_Image sf = hw_image_from(HwSheet::Background, BACKGROUND_idx);
                if (sf.tex) {
                    hw_draw_sprite(sf, (float)layout::BG_TOP_X, (float)layout::BG_TOP_Y);
                }
            }
            // Top-screen bricks pass: center horizontally via +40px X; Y is defined by layout (no extra gap)
            levels_set_draw_offset(kTopXOffset);
            levels_set_draw_offset_y(0);
            // Fill only side borders outside the brick field (background now covers top)
            {
                int cols = levels_grid_width();
                int ls = levels_left(); // includes +40 offset
                int cw = levels_brick_width();
                // Compute outer bounds: extend one brick on left/right/top
                int outerLeft = ls - cw;
                int outerRight = ls + cols * cw + cw;
                // Clamp and draw fills in top-screen coordinates (0..400 x 0..240)
                int leftW = std::max(0, std::min(outerLeft, 400));
                if (leftW > 0)
                    C2D_DrawRectSolid(0, 0, 0, (float)leftW, 240.0f, C2D_Color32(0, 0, 0, 255));
                if (outerRight < 400)
                    C2D_DrawRectSolid((float)outerRight, 0, 0, (float)(400 - outerRight), 240.0f, C2D_Color32(0, 0, 0, 255));
            }
            levels_render();
            int cols = levels_grid_width();
            int rows = levels_grid_height();
            int ls = levels_left(); // offset-aware (includes +40)
            int ts = levels_top();
            int cw = levels_brick_width();
            int ch = levels_brick_height();
            // Draw dynamic moving bricks over static grid
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
                    // Compute world-space X and clamp within world-space bounds, then convert to draw-space
                    int offX = levels_get_draw_offset(); // should be kTopXOffset here
                    float worldLeft = (float)(ls - offX);
                    float xWorld = (idx < (int)G.moving.size() && G.moving[idx].pos >= 0.f) ? G.moving[idx].pos : (worldLeft + c * cw);
                    if (idx < (int)G.moving.size())
                    {
                        if (xWorld < G.moving[idx].minX)
                            xWorld = G.moving[idx].minX;
                        if (xWorld > G.moving[idx].maxX)
                            xWorld = G.moving[idx].maxX;
                    }
                    float xDraw = xWorld + offX;
                    hw_draw_sprite(hw_image(atlas), xDraw, y);
                    if (raw == (int)BrickType::T5)
                    {
                        int hp = levels_brick_hp(c, r);
                        if (hp > 0)
                        {
                            int missing = 5 - hp;
                            int alpha = 30 + missing * 40;
                            if (alpha > 180) alpha = 180;
                            C2D_DrawRectSolid(xDraw, y, 0, cw, ch, C2D_Color32(255, 0, 0, (uint8_t)alpha));
                        }
                    }
                }
#if defined(DEBUG) && DEBUG
            // Debug colliders for static bricks
            for (int r = 0; r < rows; ++r)
                for (int c = 0; c < cols; ++c)
                {
                    int raw = levels_brick_at(c, r);
                    if (raw <= 0) continue;
                    if (is_moving_type(raw)) continue;
                    float bx = ls + c * cw;
                    float by = ts + r * ch;
                    C2D_DrawRectSolid(bx, by, 0, cw, 1, C2D_Color32(255, 0, 0, 200));
                    C2D_DrawRectSolid(bx, by + ch - 1, 0, cw, 1, C2D_Color32(255, 0, 0, 80));
                    C2D_DrawRectSolid(bx, by, 0, 1, ch, C2D_Color32(255, 0, 0, 120));
                    C2D_DrawRectSolid(bx + cw - 1, by, 0, 1, ch, C2D_Color32(255, 0, 0, 120));
                }
            // Debug colliders for moving bricks
        for (int r = 0; r < rows; ++r)
                for (int c = 0; c < cols; ++c)
                {
                    int raw = levels_brick_at(c, r);
                    if (!is_moving_type(raw)) continue;
                    int idx = r * cols + c;
                    if (idx >= (int)G.moving.size()) continue;
                    if (G.moving[idx].pos < 0.f) continue;
            int offX = levels_get_draw_offset();
            float x = G.moving[idx].pos + offX; // draw-space X on top screen
            float y = ts + r * ch;
            C2D_DrawRectSolid(x, y, 0, cw, 1, C2D_Color32(255, 0, 0, 200));
            C2D_DrawRectSolid(x, y + ch - 1, 0, cw, 1, C2D_Color32(255, 0, 0, 80));
            C2D_DrawRectSolid(x, y, 0, 1, ch, C2D_Color32(255, 0, 0, 120));
            C2D_DrawRectSolid(x + cw - 1, y, 0, 1, ch, C2D_Color32(255, 0, 0, 120));
                }
#endif
            // Light/dark overlay on top as well
            if (G.lightsOffTimer > 0) {
                C2D_DrawRectSolid(0, 0, 0, 400, 240, C2D_Color32(0, 0, 0, 140));
            }
            // HUD overlay on top screen (aligned to 320px logical area via +40px offset)
            // --- Enhanced HUD ---
            // Layout: Score (left), Level (center), Bonus (right)
            const int hudHeight = layout::HUD_HEIGHT;
            const int hudY = 0;
            const int hudX = kTopXOffset;
            const int hudW = 320;
            // Draw darker blue background bar
            C2D_DrawRectSolid(hudX, hudY, 0, hudW, hudHeight, C2D_Color32(13, 19, 54, 255));
            // Score (left)
            char scoreLabel[] = "SCORE:";
            char scoreVal[16];
            snprintf(scoreVal, sizeof scoreVal, "%06lu", G.score); // zero-padded 6 digits
            // Level (center)
            char levelLabel[] = "LEVEL:";
            char levelVal[16];
            snprintf(levelVal, sizeof levelVal, "%02d", levels_current() + 1); // pad to two digits
            // Bonus (right)
            // Removed bonus label and value text
            // Font sizes
            float labelScale = 2.0f;
            float valueScale = 2.0f;
            // Colors
            uint32_t labelColor = C2D_Color32(255, 255, 0, 255); // yellow
            uint32_t valueColor = C2D_Color32(255, 255, 255, 255); // white
            // Score left
            int scoreLabelX = hudX + 12;
            int scoreValX = scoreLabelX + hw_text_width(scoreLabel) * labelScale + 12;
            int scoreY = hudY + 6;
            hw_draw_text_shadow_scaled(scoreLabelX, scoreY, scoreLabel, labelColor, 0x000000FF, labelScale);
            hw_draw_text_shadow_scaled(scoreValX, scoreY, scoreVal, valueColor, 0x000000FF, valueScale);
            // Level right aligned
            int levelValWidth = hw_text_width(levelVal) * valueScale;
            int levelLabelWidth = hw_text_width(levelLabel) * labelScale;
            int levelLabelX = hudX + hudW - levelLabelWidth - levelValWidth - 12;
            int levelValX = hudX + hudW - levelValWidth - 8;
            hw_draw_text_shadow_scaled(levelLabelX, scoreY, levelLabel, labelColor, 0x000000FF, labelScale);
            hw_draw_text_shadow_scaled(levelValX, scoreY, levelVal, valueColor, 0x000000FF, valueScale);

            // Lives (left, second line below score)
            {
                char livesLabel[] = "Lives:";
                char livesVal[8];
                snprintf(livesVal, sizeof livesVal, "%02d", G.lives);
                int livesLabelX = scoreLabelX;
                int livesValX = livesLabelX + hw_text_width(livesLabel) * labelScale + 12;
                int livesY = scoreY + 16; // one line below score
                hw_draw_text_shadow_scaled(livesLabelX, livesY, livesLabel, labelColor, 0x000000FF, labelScale);
                hw_draw_text_shadow_scaled(livesValX, livesY, livesVal, valueColor, 0x000000FF, valueScale);
            }
            // Laser ready indicator icon on top screen (to the right of HUD text, above bonus)
            if (G.laserEnabled && G.laserReady) {
                C2D_Image ind = hw_image(IMAGE_laser_indicator_idx);
                float iw = (ind.subtex ? ind.subtex->width : 6.0f);
                float cx = (float)(kTopXOffset + 320 - 8 - iw);
                hw_draw_sprite(ind, cx, hudY + 6.0f);
            }
            // BONUS indicators: centered and moved up to appear over the blue UI background (pixel-snapped)
            {
                const int iconIdx[5] = { IMAGE_letterb_idx, IMAGE_lettero_idx, IMAGE_lettern_idx, IMAGE_letteru_idx, IMAGE_letters_idx };
                int totalW = 0;
                int heights[5] = {0,0,0,0,0};
                int widths[5] = {0,0,0,0,0};
                C2D_Image imgs[5];
                for (int i = 0; i < 5; ++i) {
                    imgs[i] = hw_image(iconIdx[i]);
                    widths[i] = (int)std::round(imgs[i].subtex ? imgs[i].subtex->width : 10.0f);
                    heights[i] = (int)std::round(imgs[i].subtex ? imgs[i].subtex->height : 11.0f);
                    totalW += widths[i];
                }
                totalW += (5 - 1) * kBonusIndicatorGapY;
                int startXi = hudX + (hudW - totalW) / 2; // integer center
                // Place BONUS row relative to HUD height; center by default with a small nudge (all integer math)
                int maxH = 0; for (int i=0;i<5;++i) if (heights[i] > maxH) maxH = heights[i];
                int yi = (int)std::round((double)hudY + (double)hudHeight * (double)layout::BONUS_Y_FACTOR - (double)maxH * 0.5 + (double)layout::BONUS_Y_OFFSET);
                int xi = startXi;
                for (int i = 0; i < 5; ++i) {
                    hw_draw_sprite(imgs[i], (float)xi, (float)yi);
                    if ((G.bonusBits & (1 << i)) == 0) {
                        C2D_DrawRectSolid((float)xi, (float)yi, 0, (float)widths[i], (float)heights[i], C2D_Color32(0, 0, 0, 140));
                    }
                    xi += widths[i] + kBonusIndicatorGapY;
                }
            }
            // Generic per-level intro (rendered on top now)
            if (G.levelIntroTimer > 0)
            {
                C2D_DrawRectSolid(0,0,0,400,240,C2D_Color32(0,0,0,120));
                const char *nm = levels_get_name(levels_current()); if (!nm) nm = "Level";
                float scale = 2.0f;
                int tw = hw_text_width(nm);
                int x = (int)((400 - tw * scale) * 0.5f);
                int y = 110 + 24; // moved down 24px
                hw_draw_text_shadow_scaled(x, y, nm, 0xFFFFFFFF, 0x000000FF, scale);
                --G.levelIntroTimer;
            }
            // Death fade overlay (top-screen copy)
            if (G.deathActive && G.deathFadeAlpha > 0) {
                int a = G.deathFadeAlpha; if (a > 200) a = 200; if (a < 0) a = 0;
                C2D_DrawRectSolid(0, 0, 0, 400, 240, C2D_Color32(0, 0, 0, (uint8_t)a));
            }
            // Game Over overlay and message on top screen
            if (G.gameOverActive) {
                int a = G.gameOverAlpha; if (a > 200) a = 200; if (a < 0) a = 0;
                C2D_DrawRectSolid(0, 0, 0, 400, 240, C2D_Color32(0, 0, 0, (uint8_t)a));
                const char *msg = "GAME OVER";
                float scale = 3.0f;
                int tw = hw_text_width(msg);
                int x = (int)((400 - tw * scale) * 0.5f);
                int y = 100; // center-ish
                hw_draw_text_shadow_scaled(x, y, msg, 0xFFFFFFFF, 0x000000FF, scale);
            }
            // Draw world side borders on the top screen for clarity (ball bounces at these walls)
            {
                int leftX  = kTopXOffset + (int)kPlayfieldLeftWallX;
                int rightX = kTopXOffset + (int)kPlayfieldRightWallX - 1;
#if defined(DEBUG) && DEBUG
                uint32_t wallCol = C2D_Color32(0, 255, 0, 160);   // collider-highlight color
#else
                uint32_t wallCol = C2D_Color32(128, 128, 128, 200); // neutral guide
#endif
                C2D_DrawRectSolid((float)leftX, 0.0f, 0, 1.0f, 240.0f, wallCol);
                C2D_DrawRectSolid((float)rightX, 0.0f, 0, 1.0f, 240.0f, wallCol);
#if defined(DEBUG) && DEBUG
                // Optional: draw top-wall collider line too
                C2D_DrawRectSolid((float)kTopXOffset, (float)kPlayfieldTopWallY, 0, 320.0f, 1.0f, C2D_Color32(0,255,0,120));
#endif
            }
            // Restore X offset for bottom pass (editor uses base; gameplay bottom uses world coords). Keep Y at 27 globally.
            levels_set_draw_offset(0);
            // Switch back to bottom for gameplay rendering
            hw_set_bottom();
        // Draw background (show lower half and center 400px-wide image on 320px bottom screen)
            {
                C2D_Image sf = hw_image_from(HwSheet::Background, BACKGROUND_idx);
                if (sf.tex) {
                    hw_draw_sprite(sf, (float)layout::BG_BOTTOM_X, (float)layout::BG_BOTTOM_Y);
                }
            }
        }
        // Editor-specific fade overlay still displays if active (bottom screen)
        if (G.mode == Mode::Playing && editor::fade_overlay_active())
            editor::render_fade_overlay();
        // Death sequence fade overlay (on top of everything else except HUD text)
        if (G.mode == Mode::Playing && G.deathActive && G.deathFadeAlpha > 0)
        {
            int a = G.deathFadeAlpha; if (a > 200) a = 200; if (a < 0) a = 0;
            C2D_DrawRectSolid(0, 0, 0, 320, 240, C2D_Color32(0, 0, 0, (uint8_t)a));
        }
        // Bottom screen darken when in Game Over to keep screens consistent
        if (G.mode == Mode::Playing && G.gameOverActive) {
            int a = G.gameOverAlpha; if (a > 200) a = 200; if (a < 0) a = 0;
            C2D_DrawRectSolid(0, 0, 0, 320, 240, C2D_Color32(0, 0, 0, (uint8_t)a));
        }
    // (Level intro moved to top-screen phase above.)
        if (G.mode == Mode::Editor)
        {
            editor::render();
            return;
        }
    if (G.mode == Mode::Options) { options::render(); return; }
    // Bricks are only rendered on the top screen now; bottom screen draws gameplay objects.
        // Draw world-space objects across both screens
        // Top screen pass for objects with y < 240
        hw_set_top();
    for (auto &p : G.particles) if (p.life > 0 && p.y < 240.0f) {
            C2D_DrawRectSolid(p.x + kTopXOffset, p.y, 0, 2, 2, p.color);
        }
        for (auto &L : G.letters) if (L.active && L.y < 240.0f) {
            hw_draw_sprite(L.img, L.x + kTopXOffset, L.y);
        }
        for (auto &H : G.hazards) if (H.active && H.y < 240.0f) {
            hw_draw_sprite(H.img, H.x + kTopXOffset, H.y);
        }
    for (auto &b : G.balls) if (b.active && b.y < 240.0f) {
        hw_draw_sprite(b.img, b.x + kTopXOffset, b.y);
#if defined(DEBUG) && DEBUG
        // Draw ball collider on top screen alongside sprite
        float spriteW = (b.img.subtex ? b.img.subtex->width : 8.f);
        float spriteH = (b.img.subtex ? b.img.subtex->height : 8.f);
        float cx = b.x + spriteW * 0.5f;
        float cy = b.y + spriteH * 0.5f;
        float lx = cx - kBallW * 0.5f;
        float ly = cy - kBallH * 0.5f;
        C2D_DrawRectSolid(lx + kTopXOffset, ly, 0, kBallW, kBallH, C2D_Color32(0, 255, 0, 90));
#endif
    }
        for (auto &LZ : G.lasers) if (LZ.active && LZ.y < 240.0f) {
            C2D_DrawRectSolid(LZ.x + kTopXOffset, LZ.y, 0, 2, 6, C2D_Color32(255,255,100,255));
        }
        // Bottom screen pass for objects with y >= 240 (subtract 240 to map to bottom viewport)
        hw_set_bottom();
        if (G.lightsOffTimer > 0) {
            C2D_DrawRectSolid(0, 0, 0, 320, 240, C2D_Color32(0, 0, 0, 140));
        }
        for (auto &p : G.particles) if (p.life > 0 && p.y >= 240.0f) {
            C2D_DrawRectSolid(p.x, p.y - 240.0f, 0, 2, 2, p.color);
        }
        for (auto &L : G.letters) if (L.active && L.y >= 240.0f) {
            hw_draw_sprite(L.img, L.x, L.y - 240.0f);
        }
        for (auto &H : G.hazards) if (H.active && H.y >= 240.0f) {
            hw_draw_sprite(H.img, H.x, H.y - 240.0f);
        }
    for (auto &b : G.balls) if (b.active && b.y >= 240.0f) {
        hw_draw_sprite(b.img, b.x, b.y - 240.0f);
#if defined(DEBUG) && DEBUG
        // Draw ball collider on bottom screen alongside sprite
        float spriteW = (b.img.subtex ? b.img.subtex->width : 8.f);
        float spriteH = (b.img.subtex ? b.img.subtex->height : 8.f);
        float cx = b.x + spriteW * 0.5f;
        float cy = b.y + spriteH * 0.5f;
        float lx = cx - kBallW * 0.5f;
        float ly = cy - kBallH * 0.5f;
        C2D_DrawRectSolid(lx, ly - 240.0f, 0, kBallW, kBallH, C2D_Color32(0, 255, 0, 90));
#endif
    }
        for (auto &LZ : G.lasers) if (LZ.active && LZ.y >= 240.0f) {
            C2D_DrawRectSolid(LZ.x, LZ.y - 240.0f, 0, 2, 6, C2D_Color32(255,255,100,255));
        }
        // Draw bat on bottom screen only
        {
            float batAtlasLeft = (G.bat.img.subtex ? G.bat.img.subtex->left : 0.0f);
            float batDrawX = G.bat.x - batAtlasLeft;
            hw_draw_sprite(G.bat.img, batDrawX, G.bat.y - 240.0f);
        }
    // Barrier line 4px high, 8px below bat. Visible for lives >= 1; hidden at 0.
    // Glow still appears even if barrier is hidden (life just dropped to 0).
    {
        float effBatH = (G.bat.img.subtex ? G.bat.img.subtex->height : G.bat.height);
    float barrierTopY = G.bat.y + effBatH + layout::BARRIER_OFFSET_BELOW_BAT;
        float barrierYBottomView = barrierTopY - 240.0f; // bottom screen coords
        float leftX = (float)kPlayfieldLeftWallX;
        float width = (float)(kPlayfieldRightWallX - kPlayfieldLeftWallX);
        if (barrier_visible())
        {
            uint32_t col = (G.lives >= 3) ? C2D_Color32(0, 200, 0, 200)
                             : (G.lives == 2) ? C2D_Color32(255, 165, 0, 220)
                             : C2D_Color32(220, 0, 0, 220);
            C2D_DrawRectSolid(leftX, barrierYBottomView, 0, width, 4.0f, col);
        }
        // Draw a momentary semi-transparent glow above the barrier if recently hit (even if barrier is now hidden at 0 lives)
        if (G.barrierGlowTimer > 0)
        {
            // Smooth ease-out over time for a soft fade
            float t = (float)G.barrierGlowTimer / (float)kBarrierGlowFrames; // 0..1
            float ease = t * t * (3.0f - 2.0f * t); // smoothstep-like
            // Alpha gradient: brightest at the top, falling off quickly
            uint8_t a0 = (uint8_t)(160 * ease);
            uint8_t a1 = (uint8_t)(90  * ease);
            uint8_t a2 = (uint8_t)(40  * ease);
            // Always white glow: 3px feathered band above the barrier top
            float glowBaseY = barrierYBottomView - layout::BARRIER_GLOW_OFFSET_ABOVE;
            C2D_DrawRectSolid(leftX, glowBaseY - 2.0f, 0, width, 1.0f, C2D_Color32(255,255,255,a0));
            C2D_DrawRectSolid(leftX, glowBaseY - 1.0f, 0, width, 1.0f, C2D_Color32(255,255,255,a1));
            C2D_DrawRectSolid(leftX, glowBaseY,        0, width, 1.0f, C2D_Color32(255,255,255,a2));
            // Optional tiny specular line right at the edge to sell the glow
            uint8_t spec = (uint8_t)(70 * ease);
            C2D_DrawRectSolid(leftX, glowBaseY - 3.0f,            0, width, 1.0f, C2D_Color32(255,255,255,spec));
            --G.barrierGlowTimer;
        }
    }
#if defined(DEBUG) && DEBUG
        // Draw world side borders (colliders) on the bottom screen as well
        {
            int leftX  = (int)kPlayfieldLeftWallX;
            int rightX = (int)kPlayfieldRightWallX - 1;
            C2D_DrawRectSolid((float)leftX, 0.0f, 0, 1.0f, 240.0f, C2D_Color32(0, 255, 0, 160));
            C2D_DrawRectSolid((float)rightX, 0.0f, 0, 1.0f, 240.0f, C2D_Color32(0, 255, 0, 160));
        }
#else
        // Non-debug: show faint side borders on bottom for clarity
        {
            int leftX  = (int)kPlayfieldLeftWallX;
            int rightX = (int)kPlayfieldRightWallX - 1;
            C2D_DrawRectSolid((float)leftX, 0.0f, 0, 1.0f, 240.0f, C2D_Color32(128, 128, 128, 200));
            C2D_DrawRectSolid((float)rightX, 0.0f, 0, 1.0f, 240.0f, C2D_Color32(128, 128, 128, 200));
        }
#endif
#if defined(DEBUG) && DEBUG
        // Draw logical bat collision rectangle (centered reduced width & height) on bottom screen
        {
            float effBatW = G.batCollWidth;
            float effBatH = (G.bat.img.subtex ? G.bat.img.subtex->height : G.bat.height);
            float batPadX = (G.bat.width - effBatW) * 0.5f;
            if (batPadX < 0) batPadX = 0;
            float batPadY = (G.bat.height - effBatH) * 0.5f;
            if (batPadY < 0) batPadY = 0;
            float batAtlasLeft2 = (G.bat.img.subtex ? G.bat.img.subtex->left : 0.0f);
            float batLeft2 = G.bat.x + batPadX - batAtlasLeft2;
            float batTop2 = G.bat.y + batPadY;
            C2D_DrawRectSolid(batLeft2, batTop2 - 240.0f, 0, effBatW, 1, C2D_Color32(255, 0, 0, 180));              // top line
            C2D_DrawRectSolid(batLeft2, batTop2 - 240.0f + effBatH - 1, 0, effBatW, 1, C2D_Color32(255, 0, 0, 80)); // bottom line
            C2D_DrawRectSolid(batLeft2, batTop2 - 240.0f, 0, 1, effBatH, C2D_Color32(255, 0, 0, 80));               // left
            C2D_DrawRectSolid(batLeft2 + effBatW - 1, batTop2 - 240.0f, 0, 1, effBatH, C2D_Color32(255, 0, 0, 80)); // right
        }
#endif
    // Ball and laser drawing handled in split top/bottom passes above
        // If in test mode (editor launched) and level ended (no breakables) or lives depleted, return to editor
        if (editor::test_return_active() && G.mode == Mode::Playing)
        {
            bool levelDone = (!editor::test_grace_active() && levels_remaining_breakable() == 0);
            if (levelDone)
            {
                G.mode = Mode::Editor;
                levels_set_current(editor::current_level_index());
                editor::on_return_from_test_full();
                hw_log("TEST return (levelDone)\n");
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
