#pragma once

#include <cstddef>

// Simple 3DS sound system using NDSP.
// - Plays 32kHz (or any PCM16) WAV SFX from romfs:/audio/
// - Streams a single music WAV from romfs:/audio/ in the background
// - Channel-based SFX playback lets you interrupt/replace an existing sound

namespace sound {

// Initialize/shutdown the audio system. Must be called from the main thread.
bool init();
void shutdown();

// Call once per frame to maintain streaming (music) and clean up finished buffers.
void update();

// Play a sound effect from ROMFS. If relativePath is true (default), path is
// treated as a file name under romfs:/audio/ and ".wav" is appended if missing.
// The provided logical channel (0..N-1) is mapped to an NDSP channel; if a sound
// is already playing on that channel it will be stopped and replaced.
bool play_sfx(const char* pathOrName, int channel, float volume = 1.0f, bool relativePath = true);

// Stop any SFX playing on a specific channel.
void stop_sfx_channel(int channel);

// Music: stream a WAV from ROMFS in the background. If relativePath is true,
// the file is opened from romfs:/audio/ and ".wav" is appended if missing.
bool play_music(const char* pathOrName, bool loop = true, float volume = 1.0f, bool relativePath = true);
void stop_music();

// Preload and cache an SFX into linear memory so the first play has no I/O or allocation cost.
bool preload_sfx(const char* pathOrName, bool relativePath = true);

}
