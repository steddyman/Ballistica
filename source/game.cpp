
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
    static constexpr int   kTiltAvailabilityFrames = 600; // 10 seconds at 60fps
    static constexpr float kTiltAngleJitter = 0.25f; // radians (~14 deg)
    static constexpr float kTiltMinSpeed = 1.2f; // ensure post-tilt velocity
    static constexpr int   kTiltShakeFrames = 30; // duration of screen shake after tilt
    static constexpr float kTiltShakeStartMag = 8.0f; // initial pixel magnitude of shake (increased for visibility)
    // Manual tuning constants for TILT arrow (bottom screen HUD). Adjust freely without touching logic.
    static constexpr float kTiltArrowScale = 1.0f;     // uniform scale applied to down_arrow.png
    static constexpr float kTiltArrowYOffset = -3.0f;   // vertical offset relative to text baseline (negative = move up)
    
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
    struct TitleBtn { UIButton btn; Mode next; bool isExit=false; };
    static TitleBtn kTitleButtons[4];

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
    // Tilt feature
    int  framesSinceBarrierHit = 0; // frames since last barrier collision
    bool tiltAvailable = false;     // true when player can trigger tilt
    int  tiltCooldownFrames = 0;    // small debounce after tilt use
    int  tiltShakeTimer = 0;        // frames remaining of tilt shake effect
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
    static void reset_positions_for_new_level();

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

    // Reset bat and ball to the standard starting positions for a fresh level
    static void reset_positions_for_new_level()
    {
        // Center bat horizontally, reset vertical position
        float batCenterX = kScreenWidth * 0.5f;
        G.bat.x = batCenterX - G.bat.width * 0.5f;
        G.bat.y = kInitialBatY;
        G.prevBatX = G.bat.x;
        // Reset to a single parked ball locked above the bat
        G.balls.clear();
        float ballStartX = kScreenWidth * 0.5f + kPlayfieldOffsetX - kInitialBallHalf;
        G.balls.push_back({ballStartX, kInitialBallY, 0.0f, 0.f, ballStartX, kInitialBallY, true, false, G.imgBall});
        G.ballLocked = true;
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
                    G.deathSinkVy += 0.1f; // accelerate
                    if (G.bat.y > 480.0f)
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
    kTitleButtons[0].btn.x=60; kTitleButtons[0].btn.y=60;  kTitleButtons[0].btn.w=200; kTitleButtons[0].btn.h=24; kTitleButtons[0].btn.label="PLAY";    kTitleButtons[0].btn.color=C2D_Color32(50,50,70,255); kTitleButtons[0].next=Mode::Playing; kTitleButtons[0].isExit=false;
    kTitleButtons[1].btn.x=60; kTitleButtons[1].btn.y=100; kTitleButtons[1].btn.w=200; kTitleButtons[1].btn.h=24; kTitleButtons[1].btn.label="EDITOR";  kTitleButtons[1].btn.color=C2D_Color32(50,50,70,255); kTitleButtons[1].next=Mode::Editor;  kTitleButtons[1].isExit=false;
    kTitleButtons[2].btn.x=60; kTitleButtons[2].btn.y=140; kTitleButtons[2].btn.w=200; kTitleButtons[2].btn.h=24; kTitleButtons[2].btn.label="OPTIONS"; kTitleButtons[2].btn.color=C2D_Color32(50,50,70,255); kTitleButtons[2].next=Mode::Options; kTitleButtons[2].isExit=false;
    // Exit button at the bottom
    kTitleButtons[3].btn.x=60; kTitleButtons[3].btn.y=200; kTitleButtons[3].btn.w=200; kTitleButtons[3].btn.h=24; kTitleButtons[3].btn.label="EXIT";    kTitleButtons[3].btn.color=C2D_Color32(60,40,40,255); kTitleButtons[3].next=Mode::Title;   kTitleButtons[3].isExit=true;
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
                    if (G.reverseTimer > 0) {
                        // Reverse already active: picking another toggles back to normal (cancel effect)
                        G.reverseTimer = 0; // stop effect and hide indicator
                        // Good outcome (controls back to normal)
                        sound::play_sfx("good", 5, 1.0f, true);
                    } else {
                        // Not active: start reverse effect
                        G.reverseTimer = 600; // ~10s
                        // Bad pickup (controls become reversed)
                        sound::play_sfx("bad", 5, 1.0f, true);
                    }
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
                        {
                            G.bombEvents.push_back({nc, nr, delay});
                            char dbg[96]; snprintf(dbg,sizeof dbg,"SCHED BOMB (%d,%d) delay=%d\n", nc,nr,delay); hw_log(dbg);
                        }
                    }
                }
    }
    static void process_bomb_events()
    {
        if (G.bombEvents.empty())
            return;
    const int ls = levels_left(), ts = levels_top(), cw = levels_brick_width(), ch = levels_brick_height();
        auto destroy_brick_immediate = [&](int c, int r) {
            int raw = levels_brick_at(c, r);
            if (raw <= 0) return;                    // empty / OOB
            if (raw == (int)BrickType::ID) return;    // indestructible
            if (raw == (int)BrickType::BO) return;    // bombs handled separately
            float cx = (float)(ls + c * cw + cw / 2);
            float cy = (float)(ts + r * ch + ch / 2);
            BrickType bt = (BrickType)raw;
            // Multi‑hit brick: treat as fully destroyed (spawn dust like final hit)
            if (bt == BrickType::T5) {
                game::spawn_dust_effect(cx, cy);
            }
            // Remove first so apply_brick_effect sees cleared grid state (consistent with resolve_hit)
            levels_remove_brick(c, r);
            apply_brick_effect(bt, cx, cy, G.balls[0]);
            if (bt == BrickType::F1 || bt == BrickType::F2) {
                spawn_destroy_bat_brick(bt, cx, cy);
            }
        };
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
            // Log neighbor states before explosion to diagnose missing destruction cases
            int rawU = levels_brick_at(ev.c,     ev.r - 1);
            int rawR = levels_brick_at(ev.c + 1, ev.r     );
            int rawD = levels_brick_at(ev.c,     ev.r + 1);
            int rawL = levels_brick_at(ev.c - 1, ev.r     );
            char dbg[96];
            snprintf(dbg, sizeof dbg, "BOMB (%d,%d) neigh U=%d R=%d D=%d L=%d\n", ev.c, ev.r, rawU, rawR, rawD, rawL);
            hw_log(dbg);
            levels_remove_brick(ev.c, ev.r);
            apply_brick_effect(BrickType::BO, ls + ev.c * cw + cw / 2, ts + ev.r * ch + ch / 2, G.balls[0]);
            // Play explosion SFX at the start of the particle effect (with fallback channel)
            if (!sound::play_sfx("explosion", 6, 1.0f, true)) {
                sound::stop_sfx_channel(6);
                sound::play_sfx("explosion", 7, 1.0f, true);
            }
            for (int k = 0; k < 8; k++)
            {
                float angle = (float)k / 8.f * 6.28318f;
                float sp = 0.6f + 0.4f * (k % 4);
                Particle p{(float)(ls + ev.c * cw + cw / 2), (float)(ts + ev.r * ch + ch / 2), std::cos(angle) * sp, std::sin(angle) * sp, 32, C2D_Color32(255, 200, 50, 255)};
                G.particles.push_back(p);
            }
            // Destroy orthogonal neighbors (Up=0, Right=1, Down=2, Left=3 semantics from legacy getside)
            destroy_brick_immediate(ev.c,     ev.r - 1); // up
            destroy_brick_immediate(ev.c + 1, ev.r    ); // right
            destroy_brick_immediate(ev.c,     ev.r + 1); // down
            destroy_brick_immediate(ev.c - 1, ev.r    ); // left
            schedule_neighbor_bombs(ev.c, ev.r, 15); // 15 frame delay

            // After this explosion (and any immediate adjacent destruction), check for required brick completion
            auto is_required_brick_local = [](BrickType bt) {
                switch(bt) {
                    case BrickType::YB: case BrickType::GB: case BrickType::CB: case BrickType::TB: case BrickType::PB: case BrickType::RB: case BrickType::SS: return true;
                    default: return false;
                }
            };
            int remainingReq = 0;
            for (int r = 0; r < kBrickRows && remainingReq == 0; ++r) {
                for (int c = 0; c < kBrickCols; ++c) {
                    int braw = levels_brick_at(c, r);
                    if (braw <= 0) continue;
                    if (is_required_brick_local((BrickType)braw)) { remainingReq = 1; break; }
                }
            }
            if (remainingReq == 0 && levels_count() > 0 && !editor::test_return_active()) {
                int next = (levels_current() + 1) % levels_count();
                levels_set_current(next);
                set_bat_size(1);
                reset_positions_for_new_level();
                G.laserEnabled = false;
                G.laserReady = false;
                hw_log("LEVEL COMPLETE\n");
            }
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
    // Stepped sweep using the ball center as a point against bricks expanded by half the ball size.
    // This reduces seam ambiguity and tunneling while keeping math simple.

        auto play_brick_sfx = [](BrickType bt, bool destroyed) {
            if (bt == BrickType::T5 && destroyed) {
                sound::stop_sfx_channel(1);
                sound::play_sfx("hard-explode", 6, 1.0f, true);
            } else {
                const char* sfx = "ball-brick";
                if (bt == BrickType::ID || bt == BrickType::SF) sfx = "hit-hard";
                else if (bt == BrickType::T5) sfx = "hit-hard"; // non-final hits
                sound::play_sfx(sfx, 1, 1.0f, true);
            }
        };

    auto is_required_brick = [](BrickType bt) {
        switch(bt) {
            case BrickType::YB:
            case BrickType::GB:
            case BrickType::CB:
            case BrickType::TB:
            case BrickType::PB:
            case BrickType::RB:
            case BrickType::SS:
                return true;
            default:
                return false;
        }
    };

    auto remaining_required_bricks = [&]() -> int {
        int cnt=0;
        int totalCols = kBrickCols;
        int totalRows = kBrickRows;
        for(int r=0;r<totalRows;++r){
            for(int c=0;c<totalCols;++c){
                int raw = levels_brick_at(c,r);
                if(raw<=0) continue;
                BrickType bt=(BrickType)raw;
                if(is_required_brick(bt)) ++cnt;
            }
        }
        return cnt;
    };

    auto resolve_hit = [&](int c, int r, float bx, float by, int cellW, int cellH, float stepDX, float stepDY) -> void {
            int raw = levels_brick_at(c, r);
            BrickType bt = (BrickType)raw;
            bool destroyed = true;
            if (bt == BrickType::T5) {
                destroyed = levels_damage_brick(c, r);
            } else if (bt == BrickType::BO) {
                levels_remove_brick(c, r);
                apply_brick_effect(BrickType::BO, bx + cellW * 0.5f, by + cellH * 0.5f, ball);
                {
                    char dbg[96]; snprintf(dbg,sizeof dbg,"HIT BOMB immediate (%d,%d) schedNow=%zu\n", c,r, G.bombEvents.size()); hw_log(dbg);
                }
                if (!sound::play_sfx("explosion", 6, 1.0f, true)) { sound::stop_sfx_channel(6); sound::play_sfx("explosion", 7, 1.0f, true); }
                for (int k = 0; k < 8; k++) {
                    float angle = (float)k / 8.f * 6.28318f;
                    float sp = 0.6f + 0.4f * (k % 4);
                    Particle p{bx + cellW * 0.5f, by + cellH * 0.5f, std::cos(angle) * sp, std::sin(angle) * sp, 32, C2D_Color32(255, 200, 50, 255)};
                    G.particles.push_back(p);
                }
                // Immediate orthogonal neighbor destruction (same rules as chain explosions)
                auto destroy_neighbor = [&](int nc, int nr) {
                    int nraw = levels_brick_at(nc, nr);
                    if (nraw <= 0) return; // empty/OOB
                    if (nraw == (int)BrickType::ID) return; // indestructible
                    if (nraw == (int)BrickType::BO) return; // bombs handled via scheduling
                    int ls = levels_left(); int ts = levels_top(); int cw = levels_brick_width(); int ch = levels_brick_height();
                    float cx = (float)(ls + nc * cw + cw * 0.5f);
                    float cy = (float)(ts + nr * ch + ch * 0.5f);
                    BrickType nbt = (BrickType)nraw;
                    if (nbt == BrickType::T5) { game::spawn_dust_effect(cx, cy); }
                    levels_remove_brick(nc, nr);
                    apply_brick_effect(nbt, cx, cy, ball);
                    if (nbt == BrickType::F1 || nbt == BrickType::F2) { spawn_destroy_bat_brick(nbt, cx, cy); }
                };
                destroy_neighbor(c, r-1); // up
                destroy_neighbor(c+1, r); // right
                destroy_neighbor(c, r+1); // down
                destroy_neighbor(c-1, r); // left
                schedule_neighbor_bombs(c, r, 15);
            } else if (bt == BrickType::ID) {
                destroyed = false;
            } else if (bt == BrickType::F1 || bt == BrickType::F2) {
                levels_remove_brick(c, r);
                apply_brick_effect(bt, bx + cellW * 0.5f, by + cellH * 0.5f, ball);
                spawn_destroy_bat_brick(bt, bx + cellW * 0.5f, by + cellH * 0.5f);
            } else {
                levels_remove_brick(c, r);
            }
            apply_brick_effect(bt, bx + cellW * 0.5f, by + cellH * 0.5f, ball);
            play_brick_sfx(bt, destroyed);

            // AB/MB: split but original continues without reflecting
            if (bt == BrickType::AB || bt == BrickType::MB) {
                if (destroyed && remaining_required_bricks() == 0 && levels_count() > 0) {
                    if(!editor::test_return_active()) {
                        int next = (levels_current() + 1) % levels_count();
                        levels_set_current(next);
                        set_bat_size(1);
                        reset_positions_for_new_level();
                        G.laserEnabled = false;
                        G.laserReady = false;
                        hw_log("LEVEL COMPLETE\n");
                    }
                }
                return; // no reflection
            }

            // Reflect using center-vs-expanded-rect distances with tie-breaker on travel axis.
            // Murder balls reflect the same as regular balls; only IS/IF are pass-through.
            if (bt != BrickType::IS && bt != BrickType::IF) {
                const float eps = 0.05f;
                float spriteW = (ball.img.subtex ? ball.img.subtex->width : (float)kBallW);
                float spriteH = (ball.img.subtex ? ball.img.subtex->height : (float)kBallH);
                float cx = ball.x + spriteW * 0.5f;
                float cy = ball.y + spriteH * 0.5f;
                float halfW = (float)kBallW * 0.5f;
                float halfH = (float)kBallH * 0.5f;
                float leftBound   = bx - halfW;
                float rightBound  = bx + cellW + halfW;
                float topBound    = by - halfH;
                float bottomBound = by + cellH + halfH;
                // Distances to each side from inside the expanded rect
                float distL = cx - leftBound;
                float distR = rightBound - cx;
                float distT = cy - topBound;
                float distB = bottomBound - cy;
                float penX = std::min(distL, distR);
                float penY = std::min(distT, distB);
                bool preferX = penX < penY;
                // Tie-break near joins: use larger travel component this substep
                if (std::fabs(penX - penY) < 0.25f) {
                    if (std::fabs(stepDX) > std::fabs(stepDY)) preferX = true;
                    else if (std::fabs(stepDY) > std::fabs(stepDX)) preferX = false;
                    // if equal, leave as computed
                }
                if (preferX) {
                    if (distL < distR) {
                        cx = leftBound - eps;
                    } else {
                        cx = rightBound + eps;
                    }
                    ball.vx = -ball.vx;
                } else {
                    if (distT < distB) {
                        cy = topBound - eps;
                    } else {
                        cy = bottomBound + eps;
                    }
                    ball.vy = -ball.vy;
                }
                // Write back top-left from updated center
                ball.x = cx - spriteW * 0.5f;
                ball.y = cy - spriteH * 0.5f;
            }

            if (destroyed && remaining_required_bricks() == 0 && levels_count() > 0) {
                if(!editor::test_return_active()) {
                    int next = (levels_current() + 1) % levels_count();
                    levels_set_current(next);
                    set_bat_size(1);
                    reset_positions_for_new_level();
                    G.laserEnabled = false;
                    G.laserReady = false;
                    hw_log("LEVEL COMPLETE\n");
                }
            }
        };

        auto step_and_check_static = [&]() -> bool {
            int ls = levels_left();
            int ts = levels_top();
            int cellW = levels_brick_width();
            int cellH = levels_brick_height();
            float targetX = ball.x, targetY = ball.y;
            float startX = ball.px, startY = ball.py;
        float dx = targetX - startX, dy = targetY - startY;
        // Substep at 0.5px increments to reduce tunneling at high speeds
        const float stepSize = 0.5f;
        int steps = (int)std::ceil(std::max(std::fabs(dx), std::fabs(dy)) / stepSize);
            if (steps < 1) steps = 1;
        float stepDX = dx / (float)steps;
        float stepDY = dy / (float)steps;
            for (int i = 1; i <= steps; ++i) {
        ball.x = startX + stepDX * (float)i;
        ball.y = startY + stepDY * (float)i;
        // Check center-point against expanded bricks at this sub-position
        float spriteW = (ball.img.subtex ? ball.img.subtex->width : (float)kBallW);
        float spriteH = (ball.img.subtex ? ball.img.subtex->height : (float)kBallH);
        float cx = ball.x + spriteW * 0.5f;
        float cy = ball.y + spriteH * 0.5f;
        float halfW = (float)kBallW * 0.5f;
        float halfH = (float)kBallH * 0.5f;
        int minCol = (int)(((cx - halfW) - ls) / cellW); if (minCol < 0) minCol = 0; if (minCol >= kBrickCols) continue;
        int maxCol = (int)((((cx + halfW) - ls)) / cellW); if (maxCol < 0) continue; if (maxCol >= kBrickCols) maxCol = kBrickCols - 1;
        int minRow = (int)(((cy - halfH) - ts) / cellH); if (minRow < 0) minRow = 0; if (minRow >= kBrickRows) continue;
        int maxRow = (int)((((cy + halfH) - ts)) / cellH); if (maxRow < 0) continue; if (maxRow >= kBrickRows) maxRow = kBrickRows - 1;
                for (int r = minRow; r <= maxRow; ++r) {
                    for (int c = minCol; c <= maxCol; ++c) {
                        int raw = levels_brick_at(c, r);
                        if (raw <= 0 || is_moving_type(raw)) continue;
            float bx = ls + c * cellW;
            float by = ts + r * cellH;
            // Expanded rect bounds
            float leftBound   = bx - halfW;
            float rightBound  = bx + cellW + halfW;
            float topBound    = by - halfH;
            float bottomBound = by + cellH + halfH;
                        if (!(cx >= leftBound && cx <= rightBound && cy >= topBound && cy <= bottomBound)) continue;
                        resolve_hit(c, r, bx, by, cellW, cellH, stepDX, stepDY);
                        return true; // stop after first for both ball types
                    }
                }
            }
            // No static hit; place ball at target
            ball.x = targetX;
            ball.y = targetY;
            return false;
        };

    auto check_moving = [&]() -> bool {
            int ts = levels_top();
            int cellW = levels_brick_width();
            int cellH = levels_brick_height();
        // Ball center at final position
        float spriteW = (ball.img.subtex ? ball.img.subtex->width : (float)kBallW);
        float spriteH = (ball.img.subtex ? ball.img.subtex->height : (float)kBallH);
        float cx = ball.x + spriteW * 0.5f;
        float cy = ball.y + spriteH * 0.5f;
        float halfW = (float)kBallW * 0.5f;
        float halfH = (float)kBallH * 0.5f;
            for (int r = 0; r < kBrickRows; ++r) {
                for (int c = 0; c < kBrickCols; ++c) {
                    int raw = levels_brick_at(c, r);
                    if (!is_moving_type(raw)) continue;
                    int idx = r * kBrickCols + c;
                    if (idx < 0 || idx >= (int)G.moving.size()) continue;
                    auto &mb = G.moving[idx];
                    if (mb.pos < 0.f) continue;
            float bx = mb.pos;
            float by = ts + r * cellH;
            float leftBound   = bx - halfW;
            float rightBound  = bx + cellW + halfW;
            float topBound    = by - halfH;
            float bottomBound = by + cellH + halfH;
            if (!(cx >= leftBound && cx <= rightBound && cy >= topBound && cy <= bottomBound)) continue;
            // Use current velocity for tie-breaking
            resolve_hit(c, r, bx, by, cellW, cellH, ball.vx, ball.vy);
            return true;
                }
            }
        return false;
        };

        if (step_and_check_static()) return;
        (void)check_moving();
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
                // Reset Tilt state so it can't appear instantly on new test
                G.framesSinceBarrierHit = 0;
                G.tiltAvailable = false;
                G.tiltCooldownFrames = 0;
                G.tiltShakeTimer = 0;
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
                for (int i = 0; i < 4; ++i)
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
                    if (tb.isExit)
                    {
                        g_exitRequested = true;
                        hw_log("exit (button)\n");
                        G.prevTouching = in.touching;
                        return;
                    }
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
                // Reset Tilt state (new level from title/debug)
                G.framesSinceBarrierHit = 0;
                G.tiltAvailable = false;
                G.tiltCooldownFrames = 0;
                G.tiltShakeTimer = 0;
                // Reinitialize moving bricks data for current layout
                int totalCells = levels_grid_width() * levels_grid_height();
                G.moving.assign(totalCells, {-1.f, 1.f, 0.f, 0.f});
                hw_log("TEST init session\n");
                G.levelIntroTimer = 90; // reuse generic intro timer (editor fade overlay still draws if active)
                // Important: reset touch edge so the release of the Test button doesn't auto-launch
                G.prevTouching = false;
                G.mode = Mode::Playing; return; }
            if (act == editor::EditorAction::SaveAndExit) { G.mode = Mode::Title; return; }
            if (in.selectPressed) { editor::persist_current_level(); G.mode = Mode::Title; return; }
            return;
        }
#ifdef __3DS__
        // In play mode, if test session launched from editor and all required bricks gone, auto-return.
        if (G.mode == Mode::Playing && editor::test_return_active() && !editor::test_grace_active()) {
            int req=0; for(int r=0;r<kBrickRows;++r){ for(int c=0;c<kBrickCols;++c){ int raw=levels_brick_at(c,r); if(raw<=0) continue; BrickType bt=(BrickType)raw; if(bt==BrickType::YB||bt==BrickType::GB||bt==BrickType::CB||bt==BrickType::TB||bt==BrickType::PB||bt==BrickType::RB||bt==BrickType::SS) ++req; }}
            if(req==0) {
                levels_reset_level(editor::current_level_index());
                editor::on_return_from_test_full();
                G.mode = Mode::Editor;
                hw_log("TEST auto-return (required cleared)\n");
                return;
            }
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
        // Update Tilt availability timing
        if (G.mode == Mode::Playing && !G.gameOverActive) {
            if (G.framesSinceBarrierHit < kTiltAvailabilityFrames + 1) ++G.framesSinceBarrierHit;
            if (!G.tiltAvailable && G.framesSinceBarrierHit >= kTiltAvailabilityFrames) G.tiltAvailable = true;
            if (G.tiltCooldownFrames > 0) --G.tiltCooldownFrames;
        }
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
    // Suppress in the brief editor test grace window to avoid accidental fire from Test tap
    if (!G.deathActive && !editor::test_grace_active() && (in.dpadUpPressed || (G.prevTouching && !in.touching)))
            fire_laser();
    // Tilt activation via D-Pad Down
    if (G.mode == Mode::Playing && G.tiltAvailable && !G.gameOverActive && in.dpadDownPressed) {
        for (auto &b : G.balls) if (b.active) {
            float speed = std::sqrt(b.vx*b.vx + b.vy*b.vy);
            if (speed < 0.01f) continue;
            uint32_t seed = (uint32_t)((uint32_t)(b.x*23) ^ (uint32_t)(b.y*37) ^ (uint32_t)G.framesSinceBarrierHit * 2654435761u);
            seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
            float rnd = (seed & 0xFFFF) / 65535.0f;
            float jitter = (rnd * 2.0f - 1.0f) * (kTiltAngleJitter * 1.5f); // stronger angle change for feedback
            float ang = std::atan2(b.vy, b.vx) + jitter;
            float newSpeed = std::max(speed * 1.15f, kTiltMinSpeed); // stronger speed boost
            b.vx = std::cos(ang) * newSpeed;
            b.vy = std::sin(ang) * newSpeed;
        }
        G.tiltAvailable = false;
        G.framesSinceBarrierHit = 0;
        G.tiltCooldownFrames = 30;
        G.tiltShakeTimer = kTiltShakeFrames;
        hw_log("TILT used\n");
        sound::play_sfx("hit-hard", 1, 1.0f, true);
    }
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
                            G.framesSinceBarrierHit = 0;
                            G.tiltAvailable = false;
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
                        // Reset Tilt availability timer on bat hit
                        G.framesSinceBarrierHit = 0;
                        G.tiltAvailable = false;
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
        // During the editor test grace window, do not consider releases to prevent auto-launch
        bool released = !editor::test_grace_active() && sawTouchWhileLocked && G.prevTouching && !in.touching; // release of that touch
            if (released)
            {
                if (!G.balls.empty())
                {
                    Ball &b0 = G.balls[0];
                    b0.vy = -1.5f;
                    float batDX = G.bat.x - G.prevBatX;
            // Clamp initial horizontal influence to avoid extremely shallow angles on quick taps
            float vx0 = batDX * 0.15f;
            if (vx0 < -1.0f) vx0 = -1.0f;
            if (vx0 >  1.0f) vx0 =  1.0f;
            b0.vx = vx0;
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
    // Advance editor test grace countdown if active
    if (editor::test_grace_active()) editor::tick_test_grace();
    }

    void render()
    {
    // --- Tilt screen shake offsets (applied to bricks & gameplay objects) ---
    int shakeX = 0, shakeY = 0;
    if (G.tiltShakeTimer > 0) {
        float t = (float)G.tiltShakeTimer / (float)kTiltShakeFrames; // 1..0
        float mag = kTiltShakeStartMag * t;
        uint32_t s = (uint32_t)G.tiltShakeTimer * 2246822519u + 3266489917u;
        s ^= s >> 13; s ^= s << 17; s ^= s >> 5;
        float rx = ((s & 0xFFFF) / 65535.0f) * 2.f - 1.f;
        float ry = (((s >> 16) & 0xFFFF) / 65535.0f) * 2.f - 1.f;
        shakeX = (int)std::round(rx * mag);
        shakeY = (int)std::round(ry * mag * 0.6f); // less vertical movement
        --G.tiltShakeTimer;
    }
    // Ensure correct brick horizontal offset per mode (no background-aligned shift anymore)
    int desiredOffset = 0; // gameplay and editor both use base left now
    if (levels_get_draw_offset() != desiredOffset) levels_set_draw_offset(desiredOffset);
    // Apply screen shake (tilt) before any world-space rendering (affects bricks & top HUD world elements)
    if (G.tiltShakeTimer > 0) {
        float t = (float)G.tiltShakeTimer / (float)kTiltShakeFrames; // 1..0
        float mag = kTiltShakeStartMag * t;
        // Simple deterministic pseudo-random each frame based on remaining timer
        uint32_t s = (uint32_t)G.tiltShakeTimer * 2246822519u + 3266489917u;
        s ^= s >> 13; s ^= s << 17; s ^= s >> 5;
        float rx = ((s & 0xFFFF) / 65535.0f) * 2.f - 1.f;
        float ry = (((s >> 16) & 0xFFFF) / 65535.0f) * 2.f - 1.f;
        int shakeX = (int)std::round(rx * mag);
        int shakeY = (int)std::round(ry * mag * 0.6f); // reduced vertical motion
        levels_set_draw_offset(desiredOffset + shakeX);
        levels_set_draw_offset_y(shakeY);
        --G.tiltShakeTimer;
        if (G.tiltShakeTimer == 0) {
            // restore defaults after finishing
            levels_set_draw_offset(desiredOffset);
            levels_set_draw_offset_y(0);
        }
    }
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
            // Top-screen bricks pass with shake
            levels_set_draw_offset(kTopXOffset + shakeX);
            levels_set_draw_offset_y(shakeY);
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
            // Reverse controls indicator (right side, second line): icon + seconds remaining
            if (G.reverseTimer > 0) {
                // Fetch icon and compute placement
                C2D_Image rev = hw_image(IMAGE_reverse_indicator_idx);
                float iw = (rev.subtex ? rev.subtex->width : 10.0f);
                float ih = (rev.subtex ? rev.subtex->height : 10.0f);
                int lineY = scoreY + 16; // align with Lives line
                // Seconds remaining, clamped to at least 1 if any frames remain
                int sec = (G.reverseTimer + 59) / 60; if (sec < 1) sec = 1;
                char buf[16]; snprintf(buf, sizeof buf, "%d", sec);
                int txtW = hw_text_width(buf) * labelScale;
                // Right-align: [ ... icon][space][NNs ] flush to hud right (hudX+hudW)
                int pad = 6;
                int textX = hudX + hudW - txtW - 8;
                int iconX = textX - (int)iw - pad;
                hw_draw_sprite(rev, (float)iconX, (float)lineY - (ih - 12.0f) * 0.5f);
                hw_draw_text_shadow_scaled(textX, lineY, buf, valueColor, 0x000000FF, labelScale);
            }
            // (Laser indicator now drawn with bat on bottom screen pass)
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
        // TILT indicator (always draws text + arrow image; image guaranteed present)
        if (G.mode == Mode::Playing && G.tiltAvailable && !G.gameOverActive) {
            const char* label = "TILT";
            const float textScale = 2.0f;
            int textW = hw_text_width(label) * textScale;
            C2D_Image arrow = hw_image(IMAGE_down_arrow_idx);
            const float arrowScale = kTiltArrowScale;
            float arrowW = (arrow.subtex ? arrow.subtex->width : 0.f) * arrowScale;
            const int gap = 6;
            int totalW = textW + gap + (int)arrowW;
            int baseX = (320 - totalW) / 2;
            int baseY = 240 - 20; // anchor
            hw_draw_text_shadow_scaled(baseX, baseY, label, 0xFFFFFFFF, 0x000000FF, textScale);
            float arrowX = (float)(baseX + textW + gap);
            float arrowY = (float)(baseY + kTiltArrowYOffset);
            C2D_DrawImageAt(arrow, arrowX, arrowY, 0.0f, nullptr, arrowScale, arrowScale);
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
            C2D_DrawRectSolid(p.x + kTopXOffset + shakeX, p.y + shakeY, 0, 2, 2, p.color);
        }
        for (auto &L : G.letters) if (L.active && L.y < 240.0f) {
            hw_draw_sprite(L.img, L.x + kTopXOffset + shakeX, L.y + shakeY);
        }
        for (auto &H : G.hazards) if (H.active && H.y < 240.0f) {
            hw_draw_sprite(H.img, H.x + kTopXOffset + shakeX, H.y + shakeY);
        }
    for (auto &b : G.balls) if (b.active && b.y < 240.0f) {
        hw_draw_sprite(b.img, b.x + kTopXOffset + shakeX, b.y + shakeY);
#if defined(DEBUG) && DEBUG
        // Draw ball collider on top screen alongside sprite
        float spriteW = (b.img.subtex ? b.img.subtex->width : 8.f);
        float spriteH = (b.img.subtex ? b.img.subtex->height : 8.f);
        float cx = b.x + spriteW * 0.5f;
        float cy = b.y + spriteH * 0.5f;
        float lx = cx - kBallW * 0.5f;
        float ly = cy - kBallH * 0.5f;
        C2D_DrawRectSolid(lx + kTopXOffset + shakeX, ly + shakeY, 0, kBallW, kBallH, C2D_Color32(0, 255, 0, 90));
#endif
    }
        for (auto &LZ : G.lasers) if (LZ.active && LZ.y < 240.0f) {
            C2D_DrawRectSolid(LZ.x + kTopXOffset + shakeX, LZ.y + shakeY, 0, 3, 10, C2D_Color32(0,255,0,255));
        }
        // Bottom screen pass for objects with y >= 240 (subtract 240 to map to bottom viewport)
        hw_set_bottom();
        if (G.lightsOffTimer > 0) {
            C2D_DrawRectSolid(0, 0, 0, 320, 240, C2D_Color32(0, 0, 0, 140));
        }
        for (auto &p : G.particles) if (p.life > 0 && p.y >= 240.0f) {
            C2D_DrawRectSolid(p.x + shakeX, p.y - 240.0f + shakeY, 0, 2, 2, p.color);
        }
        for (auto &L : G.letters) if (L.active && L.y >= 240.0f) {
            hw_draw_sprite(L.img, L.x + shakeX, L.y - 240.0f + shakeY);
        }
        for (auto &H : G.hazards) if (H.active && H.y >= 240.0f) {
            hw_draw_sprite(H.img, H.x + shakeX, H.y - 240.0f + shakeY);
        }
    for (auto &b : G.balls) if (b.active && b.y >= 240.0f) {
        hw_draw_sprite(b.img, b.x + shakeX, b.y - 240.0f + shakeY);
#if defined(DEBUG) && DEBUG
        // Draw ball collider on bottom screen alongside sprite
        float spriteW = (b.img.subtex ? b.img.subtex->width : 8.f);
        float spriteH = (b.img.subtex ? b.img.subtex->height : 8.f);
        float cx = b.x + spriteW * 0.5f;
        float cy = b.y + spriteH * 0.5f;
        float lx = cx - kBallW * 0.5f;
        float ly = cy - kBallH * 0.5f;
        C2D_DrawRectSolid(lx + shakeX, ly - 240.0f + shakeY, 0, kBallW, kBallH, C2D_Color32(0, 255, 0, 90));
#endif
    }
        for (auto &LZ : G.lasers) if (LZ.active && LZ.y >= 240.0f) {
            C2D_DrawRectSolid(LZ.x + shakeX, LZ.y - 240.0f + shakeY, 0, 3, 10, C2D_Color32(0,255,0,255));
        }
        // Draw bat on bottom screen only
        {
            float batAtlasLeft = (G.bat.img.subtex ? G.bat.img.subtex->left : 0.0f);
            float batDrawX = G.bat.x - batAtlasLeft;
            hw_draw_sprite(G.bat.img, batDrawX + shakeX, G.bat.y - 240.0f + shakeY);
            if (G.laserEnabled && G.laserReady) {
                C2D_Image ind = hw_image(IMAGE_laser_indicator_idx);
                float iw = (ind.subtex ? ind.subtex->width : 6.0f);
                float ih = (ind.subtex ? ind.subtex->height : 6.0f);
                float scale = 2.0f; // double size
                float centerX = G.bat.x + G.bat.width * 0.5f;
                float scaledW = iw * scale;
                float scaledH = ih * scale;
                float drawX = centerX - scaledW * 0.5f;
                float drawY = (G.bat.y - 240.0f) - scaledH - 2.0f; // keep same gap
                if (drawX < kPlayfieldLeftWallX) drawX = kPlayfieldLeftWallX;
                if (drawX + scaledW > kPlayfieldRightWallX) drawX = kPlayfieldRightWallX - scaledW;
                C2D_DrawImageAt(ind, drawX + shakeX, drawY + shakeY, 0.0f, nullptr, scale, scale);
            }
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
            C2D_DrawRectSolid(leftX + shakeX, barrierYBottomView + shakeY, 0, width, 4.0f, col);
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
            C2D_DrawRectSolid(leftX + shakeX, glowBaseY - 2.0f + shakeY, 0, width, 1.0f, C2D_Color32(255,255,255,a0));
            C2D_DrawRectSolid(leftX + shakeX, glowBaseY - 1.0f + shakeY, 0, width, 1.0f, C2D_Color32(255,255,255,a1));
            C2D_DrawRectSolid(leftX + shakeX, glowBaseY + shakeY,        0, width, 1.0f, C2D_Color32(255,255,255,a2));
            // Optional tiny specular line right at the edge to sell the glow
            uint8_t spec = (uint8_t)(70 * ease);
            C2D_DrawRectSolid(leftX + shakeX, glowBaseY - 3.0f + shakeY, 0, width, 1.0f, C2D_Color32(255,255,255,spec));
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
            bool levelDone = (!editor::test_grace_active());
            if(levelDone){ int req=0; for(int r=0;r<kBrickRows;++r){ for(int c=0;c<kBrickCols;++c){ int raw=levels_brick_at(c,r); if(raw<=0) continue; BrickType bt=(BrickType)raw; if(bt==BrickType::YB||bt==BrickType::GB||bt==BrickType::CB||bt==BrickType::TB||bt==BrickType::PB||bt==BrickType::RB||bt==BrickType::SS) ++req; }} levelDone = (req==0); }
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
    // Draw the title buttons using bottom-screen coordinate system.
    for (int i = 0; i < 4; ++i) {
        auto &tb = kTitleButtons[i];
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
