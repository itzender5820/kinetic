#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  audio/AudioEngine.hpp
//  Pure C++20 audio backend using OpenSL ES
//
//  Architecture:
//    • Generates PCM sine-wave tones with harmonics for 4 tracks
//    • Double-buffered playback via SLAndroidSimpleBufferQueueItf
//    • Goertzel-algorithm spectrum analysis (32 bands, 20Hz–20kHz log-spaced)
//    • Spectrum output is smoothed with exponential moving average
// ─────────────────────────────────────────────────────────────────────────────
#include "common.hpp"
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

namespace testapp {

// ── Audio constants ───────────────────────────────────────────────────────────
inline constexpr int kSampleRate    = 44100;
inline constexpr int kChannels      = 2;          // stereo
inline constexpr int kBitDepth      = 16;
inline constexpr int kFramesPerBuf  = 1024;       // frames per buffer
inline constexpr int kSamplesPerBuf = kFramesPerBuf * kChannels;
inline constexpr int kNumBuffers    = 2;           // double buffered
inline constexpr int kSpectrumBands = 32;

// ── Track descriptor ──────────────────────────────────────────────────────────
struct Track {
    std::string_view name;
    std::string_view artist;
    float  fundamental_hz;     // base tone frequency
    float  duration_sec;
    // Harmonics: relative amplitudes [fundamental, 2nd, 3rd, 4th, 5th]
    std::array<float, 5> harmonics;
};

// ── AudioEngine ───────────────────────────────────────────────────────────────
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    // Non-copyable, non-movable (holds raw SL handles)
    AudioEngine(const AudioEngine&)            = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    bool init();
    void shutdown();

    // Playback control
    bool play(int track_index);
    void pause();
    void resume();
    void stop();
    void next();
    void prev();

    // State queries
    int   current_track() const noexcept { return current_track_.load(std::memory_order_relaxed); }
    bool  is_playing()    const noexcept { return playing_.load(std::memory_order_relaxed);       }
    float position_sec()  const noexcept;
    float duration_sec()  const noexcept;
    int   track_count()   const noexcept { return static_cast<int>(kTracks.size());               }

    const Track& track_info(int index) const noexcept;

    // Thread-safe spectrum read (returns 32 values in [0,1])
    std::array<float, kSpectrumBands> spectrum_snapshot() const;

    // Catalogue of 4 built-in tracks
    static inline constexpr std::array<Track, 4> kTracks = {{
        { "Alpha Wave",   "Kinetic Audio",  432.0f, 60.0f, {0.55f, 0.25f, 0.12f, 0.05f, 0.03f} },
        { "Solar Wind",   "Kinetic Audio",  528.0f, 60.0f, {0.50f, 0.30f, 0.13f, 0.05f, 0.02f} },
        { "Deep Space",   "Kinetic Audio",  396.0f, 60.0f, {0.60f, 0.20f, 0.10f, 0.07f, 0.03f} },
        { "Cosmic Ray",   "Kinetic Audio",  639.0f, 60.0f, {0.45f, 0.28f, 0.15f, 0.08f, 0.04f} },
    }};

private:
    // ── OpenSL ES handles ─────────────────────────────────────────────────────
    SLObjectItf                    sl_engine_obj_   = nullptr;
    SLEngineItf                    sl_engine_       = nullptr;
    SLObjectItf                    sl_mix_obj_      = nullptr;
    SLObjectItf                    sl_player_obj_   = nullptr;
    SLPlayItf                      sl_play_         = nullptr;
    SLAndroidSimpleBufferQueueItf  sl_queue_        = nullptr;
    SLVolumeItf                    sl_volume_       = nullptr;

    // ── PCM buffers ───────────────────────────────────────────────────────────
    using PcmFrame  = std::array<int16_t, kSamplesPerBuf>;
    std::array<PcmFrame, kNumBuffers> pcm_bufs_{};
    int active_buf_ = 0;

    // ── Playback state ────────────────────────────────────────────────────────
    std::atomic<bool>  playing_       {false};
    std::atomic<int>   current_track_ {0};
    std::atomic<float> phase_         {0.0f};
    std::atomic<long>  sample_frames_ {0};   // total frames rendered

    // ── Spectrum state ────────────────────────────────────────────────────────
    mutable std::mutex              spectrum_mtx_;
    std::array<float, kSpectrumBands> spectrum_{};

    // ── Internal methods ──────────────────────────────────────────────────────
    bool create_player();
    void destroy_player();
    void fill_buffer(std::span<int16_t> buf);
    void compute_spectrum(std::span<const int16_t> buf);

    static void sl_callback(SLAndroidSimpleBufferQueueItf bq, void* ctx);
};

} // namespace testapp
