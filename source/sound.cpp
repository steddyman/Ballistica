#include "sound.hpp"

#include <3ds.h>
#include <3ds/ndsp/ndsp.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <cstdarg>
#include <cerrno>
#include <cstdint>
#include <unordered_map>

#ifdef PLATFORM_3DS
#include "hardware.hpp" // for hw_log
#endif

namespace sound {

static constexpr int kMaxSfxChannels = 16; // logical channels
static constexpr int kBaseNdspChannel = 0; // starting NDSP channel index for SFX
static constexpr int kMusicNdspChannel = 23; // highest valid NDSP channel for music (0..23)
static constexpr int kBrickSfxChannel = 1;   // logical channel reserved for ball-brick/wall hits

struct SfxState {
    ndspWaveBuf wave{};
    // Channel-local view; underlying memory is owned by the global cache.
    void* data = nullptr;      // pointer to cached PCM16 interleaved data (linear memory)
    size_t bytes = 0;
    int channels = 0;
    bool active = false;
};

static SfxState g_sfx[kMaxSfxChannels];

struct MusicState {
    FILE* f = nullptr;
    ndspWaveBuf wave[2]{}; // simple double buffer
    int16_t* buf[2] = {nullptr, nullptr}; // linear-allocated buffers
    size_t framesPerBuf = 0; // per-channel frames in each buffer
    size_t bytesPerSample = 2; // PCM16
    int sampleRate = 32000;
    int channels = 2;
    bool looping = true;
    bool active = false;
    int cur = 0;
    long dataStart = 0;
    size_t dataBytes = 0;
    size_t bytesLeft = 0;
};

static MusicState g_music;
static bool g_inited = false;
static bool g_warnedNoInit = false;
// Per-channel debounce timestamp (ms since boot). Prevents rapid stacking when many hits occur at once.
static uint64_t g_lastSfxPlayMs[kMaxSfxChannels] = {0};
static constexpr uint32_t kSfxDebounceMs = 35; // minimum gap between plays on the same logical channel

// SFX cache: keep each SFX loaded once in linear memory and reuse it.
struct Clip {
    void* data = nullptr;   // linear-allocated PCM16 interleaved data
    size_t bytes = 0;
    int rate = 32000;
    int channels = 1;       // 1 or 2
    u32 nsamples = 0;       // per-channel frames
};
static std::unordered_map<std::string, Clip> g_clipCache; // key: resolved romfs path

static inline uint64_t now_ms() {
#ifdef PLATFORM_3DS
    return (uint64_t)osGetTime();
#else
    using namespace std::chrono;
    return (uint64_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
#endif
}

// Lightweight debug logging helper
static void dbg_logf(const char* fmt, ...) {
#if defined(DEBUG)
#  ifdef PLATFORM_3DS
    char buf[256];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    hw_log(buf);
#  else
    (void)fmt;
#  endif
#else
    (void)fmt;
#endif
}

static bool ensure_romfs_prefix(std::string &out, const char* pathOrName, bool relative, const char* subdir) {
    if (!pathOrName || !*pathOrName) return false;
    out.clear();
    if (relative) {
        out = std::string("romfs:/") + subdir + "/" + pathOrName;
    } else {
        out = pathOrName;
    }
    if (out.size() < 4 || out.substr(out.size()-4) != ".wav") out += ".wav";
    dbg_logf("snd path: %s (rel=%d)\n", out.c_str(), relative ? 1 : 0);
    return true;
}

// Minimal WAV reader: PCM16, mono or stereo, returns interleaved PCM16 and sample rate
static bool load_wav_pcm16(const char* path, std::vector<int16_t>& outPcm, int& outRate, int& outChannels, long& outDataStart, size_t& outDataBytes) {
    outPcm.clear(); outRate = 0; outChannels = 0; outDataStart = 0; outDataBytes = 0;
    FILE* f = fopen(path, "rb");
    if (!f) { dbg_logf("wav open fail: %s (errno=%d)\n", path, errno); return false; }
    char id[4]; uint32_t size;
    if (fread(id,1,4,f)!=4 || strncmp(id,"RIFF",4)!=0) { fclose(f); dbg_logf("wav not RIFF: %s\n", path); return false; }
    fseek(f,4,SEEK_CUR); // skip RIFF size
    if (fread(id,1,4,f)!=4 || strncmp(id,"WAVE",4)!=0) { fclose(f); dbg_logf("wav not WAVE: %s\n", path); return false; }
    bool fmtFound=false, dataFound=false; uint16_t fmt=0; uint32_t sr=0; uint16_t ch=0; uint16_t bps=0;
    long dataPos=0; uint32_t dataSize=0;
    while (fread(id,1,4,f)==4 && fread(&size,4,1,f)==1) {
        if (strncmp(id,"fmt ",4)==0) {
            fmtFound=true; uint16_t blockAlign=0; uint32_t byteRate=0;
            fread(&fmt,2,1,f); fread(&ch,2,1,f); fread(&sr,4,1,f); fread(&byteRate,4,1,f); fread(&blockAlign,2,1,f); fread(&bps,2,1,f);
            fseek(f, size - 16, SEEK_CUR);
        } else if (strncmp(id,"data",4)==0) {
            dataFound=true; dataPos = ftell(f); dataSize = size; fseek(f, size, SEEK_CUR);
        } else {
            fseek(f, size, SEEK_CUR);
        }
    }
    if (!fmtFound || !dataFound || fmt != 1 || bps != 16) { fclose(f); dbg_logf("wav unsupported: %s (fmt=%u bps=%u fmtFound=%d dataFound=%d)\n", path, (unsigned)fmt, (unsigned)bps, fmtFound?1:0, dataFound?1:0); return false; }
    outRate = (int)sr; outChannels = (int)ch; outDataStart = dataPos; outDataBytes = dataSize;
    // Read entire file into memory for SFX usage
    outPcm.resize(dataSize / 2);
    fseek(f, dataPos, SEEK_SET);
    size_t got = fread(outPcm.data(), 1, dataSize, f);
    if (got != dataSize) { dbg_logf("wav short read: %s (%zu/%u)\n", path, got, dataSize); }
    fclose(f);
    dbg_logf("wav ok: %s rate=%d ch=%d bytes=%u\n", path, outRate, outChannels, (unsigned)outDataBytes);
    return true;
}

bool init() {
    if (g_inited) return true;
    if (ndspInit() != 0) {
    dbg_logf("ndspInit failed\n");
#ifdef PLATFORM_3DS
    // Probe for DSP firmware dump (required for 3DSX homebrew)
    FILE* df = fopen("sdmc:/3ds/dspfirm.cdc", "rb");
    if (df) { fclose(df); dbg_logf("dspfirm.cdc present at sdmc:/3ds/dspfirm.cdc (NDSP still failed)\n"); }
    else { dbg_logf("dspfirm.cdc NOT found at sdmc:/3ds/dspfirm.cdc\n"); }
    dbg_logf("Audio disabled. On real 3DS: run 'DSP1' dumper to create dspfirm.cdc. On Citra: ensure audio is enabled.\n");
#endif
    return false;
    }
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspSetMasterVol(1.0f);
    for (int i = 0; i < kMaxSfxChannels; ++i) {
        int ndspCh = kBaseNdspChannel + i;
        ndspChnReset(ndspCh);
        ndspChnSetInterp(ndspCh, NDSP_INTERP_NONE);
        ndspChnSetRate(ndspCh, 32000.0f);
        ndspChnSetFormat(ndspCh, NDSP_FORMAT_MONO_PCM16);
    }
    // Music channel setup
    ndspChnReset(kMusicNdspChannel);
    ndspChnSetInterp(kMusicNdspChannel, NDSP_INTERP_NONE);
    ndspChnSetRate(kMusicNdspChannel, 32000.0f);
    ndspChnSetFormat(kMusicNdspChannel, NDSP_FORMAT_STEREO_PCM16);
    g_inited = true;
    dbg_logf("sound init ok (musicCh=%d)\n", kMusicNdspChannel);
    return true;
}

void shutdown() {
    if (!g_inited) return;
    stop_music();
    for (int i = 0; i < kMaxSfxChannels; ++i) stop_sfx_channel(i);
    // Free cached clips
    for (auto &kv : g_clipCache) { if (kv.second.data) linearFree(kv.second.data); }
    g_clipCache.clear();
    ndspExit();
    g_inited = false;
    dbg_logf("sound shutdown\n");
}

void update() {
    if (!g_inited) return;
    // Music streaming: refill finished buffers
    if (g_music.active && g_music.f) {
        ndspWaveBuf* wb = &g_music.wave[g_music.cur];
        if (wb->status == NDSP_WBUF_DONE) {
            // Refill or loop
        size_t bytesToRead = g_music.framesPerBuf * g_music.channels * sizeof(int16_t);
        size_t read = fread(g_music.buf[g_music.cur], 1, bytesToRead, g_music.f);
            if (read < bytesToRead) {
                if (g_music.looping) {
                    fseek(g_music.f, g_music.dataStart, SEEK_SET);
                    size_t rem = bytesToRead - read;
            fread(((uint8_t*)g_music.buf[g_music.cur]) + read, 1, rem, g_music.f);
            dbg_logf("music loop refill buf=%d read=%zu+%zu\n", g_music.cur, read, rem);
                } else {
                    g_music.active = false;
            dbg_logf("music end reached\n");
                    return;
                }
            }
            // Queue again
        wb->nsamples = (u32)(g_music.framesPerBuf); // frames per channel
        wb->data_vaddr = g_music.buf[g_music.cur];
            DSP_FlushDataCache(wb->data_vaddr, bytesToRead);
            ndspChnWaveBufAdd(kMusicNdspChannel, wb);
        dbg_logf("music queue buf=%d nsamples=%u\n", g_music.cur, wb->nsamples);
            g_music.cur ^= 1;
        }
    }
}

static void stop_sfx_internal(int channel) {
    int ndspCh = kBaseNdspChannel + channel;
    ndspChnWaveBufClear(ndspCh);
    // Do not free memory; owned by cache
    g_sfx[channel].data = nullptr;
    g_sfx[channel].bytes = 0;
    g_sfx[channel].channels = 0;
    g_sfx[channel].active = false;
    dbg_logf("sfx stop ch=%d (ndsp=%d)\n", channel, ndspCh);
}

void stop_sfx_channel(int channel) {
    if (channel < 0 || channel >= kMaxSfxChannels) return;
    stop_sfx_internal(channel);
}

bool play_sfx(const char* pathOrName, int channel, float volume, bool relativePath) {
    if (!g_inited) { if (!g_warnedNoInit) { dbg_logf("audio disabled (init failed); skipping sfx\n"); g_warnedNoInit = true; } return false; }
    if (channel < 0) channel = 0;
    if (channel >= kMaxSfxChannels) channel = kMaxSfxChannels-1;
    uint64_t t = now_ms();
    if (channel == kBrickSfxChannel) {
        if (t - g_lastSfxPlayMs[channel] < kSfxDebounceMs) {
            dbg_logf("sfx debounce ch=%d dt=%llu<%u\n", channel, (unsigned long long)(t - g_lastSfxPlayMs[channel]), kSfxDebounceMs);
            return false;
        }
    }
    std::string path;
    if (!ensure_romfs_prefix(path, pathOrName, relativePath, "audio")) { dbg_logf("sfx bad path\n"); return false; }
    // Lookup / load into cache
    Clip* clip = nullptr;
    auto it = g_clipCache.find(path);
    if (it == g_clipCache.end()) {
        int rate=0, ch=0; long dataStart=0; size_t dataBytes=0; std::vector<int16_t> pcm;
        if (!load_wav_pcm16(path.c_str(), pcm, rate, ch, dataStart, dataBytes)) { dbg_logf("sfx load fail: %s\n", path.c_str()); return false; }
        Clip c{}; c.bytes = pcm.size() * sizeof(int16_t); c.rate = rate; c.channels = ch; c.nsamples = (u32)(pcm.size() / (ch ? ch : 1));
        c.data = linearAlloc(c.bytes);
        if (!c.data) { dbg_logf("sfx linearAlloc fail (%zu)\n", c.bytes); return false; }
        memcpy(c.data, pcm.data(), c.bytes);
        DSP_FlushDataCache(c.data, c.bytes);
        auto res = g_clipCache.emplace(path, c);
        if (!res.second) { linearFree(c.data); return false; }
        clip = &res.first->second;
        dbg_logf("sfx cached: %s bytes=%zu rate=%d ch=%d nsamp=%u\n", path.c_str(), clip->bytes, clip->rate, clip->channels, clip->nsamples);
    } else {
        clip = &it->second;
    }
    int ndspCh = kBaseNdspChannel + channel;
    ndspChnReset(ndspCh);
    ndspChnSetInterp(ndspCh, NDSP_INTERP_NONE);
    ndspChnSetRate(ndspCh, (float)clip->rate);
    ndspChnSetFormat(ndspCh, clip->channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
    stop_sfx_internal(channel);
    SfxState &S = g_sfx[channel];
    S.channels = clip->channels;
    S.bytes = clip->bytes;
    S.data = clip->data;
    memset(&S.wave, 0, sizeof(S.wave));
    S.wave.data_vaddr = S.data;
    S.wave.nsamples   = clip->nsamples;
    S.wave.looping    = false;
    {
        float mix[12] = {0}; mix[0] = volume; mix[1] = volume; ndspChnSetMix(ndspCh, mix);
    }
    ndspChnWaveBufAdd(ndspCh, &S.wave);
    S.active = true; if (channel == kBrickSfxChannel) g_lastSfxPlayMs[channel] = t;
    dbg_logf("sfx play ch=%d ndsp=%d rate=%d ch=%d nsamp=%u vol=%.2f (cached)\n", channel, ndspCh, clip->rate, clip->channels, S.wave.nsamples, volume);
    return true;
}

void stop_music() {
    if (g_music.active) {
        ndspChnWaveBufClear(kMusicNdspChannel);
        g_music.active = false;
    dbg_logf("music stop\n");
    }
    if (g_music.f) { fclose(g_music.f); g_music.f = nullptr; }
    for (int i=0;i<2;++i) { if (g_music.buf[i]) { linearFree(g_music.buf[i]); g_music.buf[i]=nullptr; } memset(&g_music.wave[i],0,sizeof(ndspWaveBuf)); }
}

bool play_music(const char* pathOrName, bool loop, float volume, bool relativePath) {
    if (!g_inited) { if (!g_warnedNoInit) { dbg_logf("audio disabled (init failed); skipping music\n"); g_warnedNoInit = true; } return false; }
    stop_music();
    std::string path; if (!ensure_romfs_prefix(path, pathOrName, relativePath, "audio")) return false;
    dbg_logf("music request: %s loop=%d vol=%.2f rel=%d\n", path.c_str(), loop?1:0, volume, relativePath?1:0);
    // Parse WAV header but do streaming (don’t load full file)
    std::vector<int16_t> tmp; int rate=0, ch=0; long dataStart=0; size_t dataBytes=0;
    if (!load_wav_pcm16(path.c_str(), tmp, rate, ch, dataStart, dataBytes)) { dbg_logf("music load fail: %s\n", path.c_str()); return false; }
    // Reopen for streaming and seek to data
    g_music.f = fopen(path.c_str(), "rb"); if (!g_music.f) { dbg_logf("music fopen fail: %s (errno=%d)\n", path.c_str(), errno); return false; }
    fseek(g_music.f, dataStart, SEEK_SET);
    g_music.looping = loop; g_music.sampleRate = rate; g_music.channels = ch; g_music.dataStart = dataStart; g_music.dataBytes = dataBytes; g_music.bytesLeft = dataBytes;
    // Setup channel format
    ndspChnReset(kMusicNdspChannel);
    ndspChnSetInterp(kMusicNdspChannel, NDSP_INTERP_NONE);
    ndspChnSetRate(kMusicNdspChannel, (float)rate);
    ndspChnSetFormat(kMusicNdspChannel, ch == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
    {
        float mix[12] = {0};
        mix[0] = volume; mix[1] = volume;
        ndspChnSetMix(kMusicNdspChannel, mix);
    }
    // Allocate two streaming buffers (~0.25s each) in linear memory
    const size_t frames = (size_t)(rate / 4); // 0.25s worth of frames
    g_music.framesPerBuf = frames;
    for (int i=0;i<2;++i) {
        size_t samples = frames * ch;
        size_t bytes = samples * sizeof(int16_t);
        g_music.buf[i] = (int16_t*)linearAlloc(bytes);
        if (!g_music.buf[i]) { dbg_logf("music linearAlloc fail buf=%d (%zu bytes)\n", i, bytes); stop_music(); return false; }
        memset(&g_music.wave[i], 0, sizeof(ndspWaveBuf));
    }
    // Prime both buffers
    for (int i=0;i<2;++i) {
        size_t toRead = g_music.framesPerBuf * g_music.channels * sizeof(int16_t);
        size_t got = fread(g_music.buf[i], 1, toRead, g_music.f);
        if (got < toRead && g_music.looping) {
            fseek(g_music.f, g_music.dataStart, SEEK_SET);
            fread(((uint8_t*)g_music.buf[i]) + got, 1, toRead - got, g_music.f);
        }
        g_music.wave[i].data_vaddr = g_music.buf[i];
        g_music.wave[i].nsamples = (u32)(g_music.framesPerBuf);
        DSP_FlushDataCache(g_music.wave[i].data_vaddr, toRead);
    ndspChnWaveBufAdd(kMusicNdspChannel, &g_music.wave[i]);
        dbg_logf("music prime buf=%d bytes=%zu nsamples=%u rate=%d ch=%d vol=%.2f chn=%d\n", i, toRead, g_music.wave[i].nsamples, rate, ch, volume, kMusicNdspChannel);
    }
    g_music.cur = 0; g_music.active = true;
    dbg_logf("music play: %s loop=%d\n", path.c_str(), loop?1:0);
    return true;
}

bool preload_sfx(const char* pathOrName, bool relativePath) {
    if (!g_inited) return false;
    std::string path; if (!ensure_romfs_prefix(path, pathOrName, relativePath, "audio")) return false;
    if (g_clipCache.find(path) != g_clipCache.end()) return true; // already cached
    int rate=0, ch=0; long dataStart=0; size_t dataBytes=0; std::vector<int16_t> pcm;
    if (!load_wav_pcm16(path.c_str(), pcm, rate, ch, dataStart, dataBytes)) return false;
    Clip c{}; c.bytes = pcm.size() * sizeof(int16_t); c.rate = rate; c.channels = ch; c.nsamples = (u32)(pcm.size() / (ch ? ch : 1));
    c.data = linearAlloc(c.bytes); if (!c.data) return false;
    memcpy(c.data, pcm.data(), c.bytes); DSP_FlushDataCache(c.data, c.bytes);
    g_clipCache.emplace(path, c);
    dbg_logf("preloaded sfx: %s bytes=%zu rate=%d ch=%d nsamp=%u\n", path.c_str(), c.bytes, rate, ch, c.nsamples);
    return true;
}

} // namespace sound
