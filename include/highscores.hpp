#pragma once
#include <cstdint>

// Simple high score table management for 3DS port.
// Stored on sdmc:/scores.dat (not in RomFS) in a simple text-friendly binary format:
// For each entry (NUM_SCORES=8): uint32_t score, uint8_t level, newline-terminated name string.

namespace highscores {
    static const int NUM_SCORES = 10;
    static const int MAX_NAME = 10; // 10 chars + null

    struct Entry { uint32_t score; uint8_t level; char name[MAX_NAME+1]; };

    // Load or create defaults.
    void init();
    // Attempt to insert new score; returns index (0..NUM_SCORES-1) or -1 if not placed.
    int submit(uint32_t score, int level);
    // Set name for an existing entry (truncate automatically).
    void set_name(int index, const char* name);
    // Persist table.
    void save();
    // Access immutable array pointer.
    const Entry* table();
}
