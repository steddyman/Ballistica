#pragma once

// Centralized layout constants for Ballistica
// All gameplay, rendering, and collision logic should use these

namespace layout {
    // UI and playfield
    static constexpr int HUD_HEIGHT = 36; // Height of blue score UI
    static constexpr int UI_BRICK_OFFSET = 16; // Pixels below UI to start brick grid
    static constexpr int BRICK_GRID_TOP = HUD_HEIGHT + UI_BRICK_OFFSET; // Top Y of brick grid
    static constexpr int BRICK_CELL_W = 22;
    static constexpr int BRICK_CELL_H = 13;
    static constexpr int BRICK_GRID_LEFT = 17; // Centered horizontally
    static constexpr int PLAYFIELD_LEFT_WALL_X = 0;
    static constexpr int PLAYFIELD_RIGHT_WALL_X = 320 - 1;
    static constexpr int PLAYFIELD_TOP_WALL_Y = HUD_HEIGHT; // Top wall matches UI bottom
    static constexpr int SCREEN_WIDTH = 320;
    static constexpr int TOP_X_OFFSET = 40; // For top screen centering

    // Background image placement (single tall background drawn on both screens)
    static constexpr int BG_TOP_X = 40;     // top screen background X (center 320px on 400px)
    static constexpr int BG_TOP_Y = 0;      // top screen background Y
    static constexpr int BG_BOTTOM_X = 0;   // bottom screen background X (full width at x=0)
    static constexpr int BG_BOTTOM_Y = -240; // bottom shows lower half of a 480px region

    // BONUS indicators vertical placement within the HUD bar
    // y = hudY + HUD_HEIGHT * BONUS_Y_FACTOR - (maxIconH * 0.5f) + BONUS_Y_OFFSET
    // Set BONUS_Y_FACTOR to 0.5f for center, 0.0f for top, 1.0f for bottom.
    static constexpr float BONUS_Y_FACTOR = 0.5f;
    static constexpr int   BONUS_Y_OFFSET = 10; // fine-tune nudge in pixels

    // Initial positions for bat and ball
    static constexpr float kInitialBatY = 420.0f; // 220 + 240, bottom screen space
    static constexpr float kInitialBallY = kInitialBatY - 8.0f; // Ball just above bat
    static constexpr float kInitialBallHalf = 3.0f; // Half ball width (if ball is 6px)

    // Gameplay tuning
    // Speed modifier for speed-affecting bricks (value 0.05f means +/-5% per effect)
    static constexpr float SPEED_MODIFIER = 0.05f;
}
