// ─────────────────────────────────────────────────────────────────────────────
//  audio/AudioEngine.cpp
// ─────────────────────────────────────────────────────────────────────────────
#include "AudioEngine.hpp"
#include <cstring>
#include <numbers>

namespace testapp {

// ── Constructor / Destructor ──────────────────────────────────────────────────
AudioEngine::AudioEngine() {
    spectrum_.fill(0.0f);
}

AudioEngine::~AudioEngine() {
    shutdown();
}

// ── init ─────────────────────────────────────────────────────────────────────
bool AudioEngine::init() {
    // Create OpenSL ES engine
    SLresult r = slCreateEngine(&sl_engine_obj_, 0, nullptr, 0, nullptr, nullptr);
    if (r != SL_RESULT_SUCCESS) { LOGE("slCreateEngine failed: %d", r); return false; }

    r = (*sl_engine_obj_)->Realize(sl_engine_obj_, SL_BOOLEAN_FALSE);
    if (r != SL_RESULT_SUCCESS) { LOGE("Engine Realize failed: %d", r); return false; }

    r = (*sl_engine_obj_)->GetInterface(sl_engine_obj_, SL_IID_ENGINE, &sl_engine_);
    if (r != SL_RESULT_SUCCESS) { LOGE("GetInterface ENGINE failed: %d", r); return false; }

    // Create output mix
    r = (*sl_engine_)->CreateOutputMix(sl_engine_, &sl_mix_obj_, 0, nullptr, nullptr);
    if (r != SL_RESULT_SUCCESS) { LOGE("CreateOutputMix failed: %d", r); return false; }

    r = (*sl_mix_obj_)->Realize(sl_mix_obj_, SL_BOOLEAN_FALSE);
    if (r != SL_RESULT_SUCCESS) { LOGE("OutputMix Realize failed: %d", r); return false; }

    LOGI("AudioEngine: OpenSL ES initialized");
    return true;
}

// ── shutdown ──────────────────────────────────────────────────────────────────
void AudioEngine::shutdown() {
    destroy_player();
    if (sl_mix_obj_)    { (*sl_mix_obj_)->Destroy(sl_mix_obj_);       sl_mix_obj_ = nullptr;    }
    if (sl_engine_obj_) { (*sl_engine_obj_)->Destroy(sl_engine_obj_); sl_engine_obj_ = nullptr; }
    LOGI("AudioEngine: shutdown complete");
}

// ── create_player ─────────────────────────────────────────────────────────────
bool AudioEngine::create_player() {
    // Data source: AndroidSimpleBufferQueue, PCM stereo 44100Hz 16-bit
    SLDataLocator_AndroidSimpleBufferQueue loc_bq = {
        SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
        (SLuint32)kNumBuffers
    };
    SLDataFormat_PCM fmt = {
        SL_DATAFORMAT_PCM,
        (SLuint32)kChannels,
        SL_SAMPLINGRATE_44_1,
        SL_PCMSAMPLEFORMAT_FIXED_16,
        SL_PCMSAMPLEFORMAT_FIXED_16,
        SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
        SL_BYTEORDER_LITTLEENDIAN
    };
    SLDataSource source = { &loc_bq, &fmt };

    // Data sink: output mix
    SLDataLocator_OutputMix loc_mix = { SL_DATALOCATOR_OUTPUTMIX, sl_mix_obj_ };
    SLDataSink sink = { &loc_mix, nullptr };

    // Interfaces to request
    const SLInterfaceID ids[2]  = { SL_IID_BUFFERQUEUE, SL_IID_VOLUME };
    const SLboolean     req[2]  = { SL_BOOLEAN_TRUE,    SL_BOOLEAN_TRUE };

    SLresult r = (*sl_engine_)->CreateAudioPlayer(
        sl_engine_, &sl_player_obj_, &source, &sink, 2, ids, req);
    if (r != SL_RESULT_SUCCESS) { LOGE("CreateAudioPlayer failed: %d", r); return false; }

    r = (*sl_player_obj_)->Realize(sl_player_obj_, SL_BOOLEAN_FALSE);
    if (r != SL_RESULT_SUCCESS) { LOGE("Player Realize failed: %d", r); return false; }

    r = (*sl_player_obj_)->GetInterface(sl_player_obj_, SL_IID_PLAY, &sl_play_);
    if (r != SL_RESULT_SUCCESS) { LOGE("GetInterface PLAY failed: %d", r); return false; }

    r = (*sl_player_obj_)->GetInterface(sl_player_obj_, SL_IID_BUFFERQUEUE, &sl_queue_);
    if (r != SL_RESULT_SUCCESS) { LOGE("GetInterface BUFFERQUEUE failed: %d", r); return false; }

    r = (*sl_player_obj_)->GetInterface(sl_player_obj_, SL_IID_VOLUME, &sl_volume_);
    if (r != SL_RESULT_SUCCESS) { LOGD("GetInterface VOLUME not available (non-fatal)"); }

    // Register callback
    r = (*sl_queue_)->RegisterCallback(sl_queue_, sl_callback, this);
    if (r != SL_RESULT_SUCCESS) { LOGE("RegisterCallback failed: %d", r); return false; }

    return true;
}

// ── destroy_player ────────────────────────────────────────────────────────────
void AudioEngine::destroy_player() {
    playing_.store(false);
    if (sl_player_obj_) {
        (*sl_player_obj_)->Destroy(sl_player_obj_);
        sl_player_obj_ = nullptr;
        sl_play_       = nullptr;
        sl_queue_      = nullptr;
        sl_volume_     = nullptr;
    }
}

// ── fill_buffer ───────────────────────────────────────────────────────────────
// Synthesise one buffer of stereo PCM using additive synthesis.
// Each track has a fundamental tone + 4 harmonics at defined relative amplitudes.
void AudioEngine::fill_buffer(std::span<int16_t> buf) {
    const int idx = current_track_.load(std::memory_order_relaxed);
    const Track& t  = kTracks[idx];
    const float  fs = static_cast<float>(kSampleRate);
    const float  dt = 1.0f / fs;

    float phase = phase_.load(std::memory_order_relaxed);
    long  sc    = sample_frames_.load(std::memory_order_relaxed);

    for (int i = 0; i < (int)buf.size(); i += kChannels) {
        // Additive synthesis: fundamental + harmonics
        float s = 0.0f;
        for (int h = 0; h < 5; ++h) {
            float freq = t.fundamental_hz * static_cast<float>(h + 1);
            s += t.harmonics[h] * std::sin(2.0f * std::numbers::pi_v<float> * freq * phase);
        }

        // Amplitude envelope: fade in (0.15 s) and fade out (last 0.15 s)
        const float pos = static_cast<float>(sc) / fs;
        const float dur = t.duration_sec;
        const float fade_sec = 0.15f;
        float env = testapp::clamp(
            std::min(pos / fade_sec, (dur - pos) / fade_sec),
            0.0f, 1.0f);

        // Slight amplitude wobble (vibrato-like effect)
        env *= (0.92f + 0.08f * std::sin(2.0f * std::numbers::pi_v<float> * 5.3f * pos));

        const auto s16 = static_cast<int16_t>(s * env * 24000.0f);
        buf[i]   = s16;  // L
        buf[i+1] = s16;  // R

        phase += dt;
        if (phase >= 1.0f) phase -= 1.0f;
        ++sc;

        // Loop track
        if (pos >= dur) {
            sc = 0;
            phase = 0.0f;
        }
    }

    phase_.store(phase, std::memory_order_relaxed);
    sample_frames_.store(sc, std::memory_order_relaxed);
}

// ── compute_spectrum ──────────────────────────────────────────────────────────
// Goertzel algorithm: compute power at each of kSpectrumBands frequency bins
// spaced logarithmically from 20 Hz to 20 kHz.
void AudioEngine::compute_spectrum(std::span<const int16_t> buf) {
    constexpr float lo = 20.0f;
    constexpr float hi = 20000.0f;
    const float log_lo = std::log10(lo);
    const float log_hi = std::log10(hi);
    const int   frames = static_cast<int>(buf.size()) / kChannels;

    std::array<float, kSpectrumBands> bands{};

    for (int band = 0; band < kSpectrumBands; ++band) {
        const float t    = static_cast<float>(band) / (kSpectrumBands - 1);
        const float freq = std::pow(10.0f, log_lo + (log_hi - log_lo) * t);
        const float omega = 2.0f * std::numbers::pi_v<float> * freq / static_cast<float>(kSampleRate);
        const float coeff = 2.0f * std::cos(omega);

        float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f;
        for (int i = 0; i < frames; ++i) {
            const float x = static_cast<float>(buf[i * kChannels]) / 32768.0f;
            s0 = x + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        // Goertzel power
        const float power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
        // Normalise: sqrt(power) / frames, then scale for visual impact
        bands[band] = testapp::clamp(
            std::sqrt(std::max(0.0f, power)) / static_cast<float>(frames) * 6.0f,
            0.0f, 1.0f);
    }

    // Exponential smoothing (fast attack, slow decay)
    std::lock_guard lock(spectrum_mtx_);
    for (int i = 0; i < kSpectrumBands; ++i) {
        const float alpha = bands[i] > spectrum_[i] ? 0.60f : 0.15f;
        spectrum_[i] = alpha * bands[i] + (1.0f - alpha) * spectrum_[i];
    }
}

// ── sl_callback ───────────────────────────────────────────────────────────────
void AudioEngine::sl_callback(SLAndroidSimpleBufferQueueItf /*bq*/, void* ctx) {
    auto* self = static_cast<AudioEngine*>(ctx);
    if (!self->playing_.load(std::memory_order_acquire)) return;

    const int buf_idx = self->active_buf_;
    self->active_buf_ = (buf_idx + 1) % kNumBuffers;

    auto& frame = self->pcm_bufs_[buf_idx];
    self->fill_buffer(std::span<int16_t>(frame.data(), frame.size()));
    self->compute_spectrum(std::span<const int16_t>(frame.data(), frame.size()));

    (*self->sl_queue_)->Enqueue(
        self->sl_queue_,
        frame.data(),
        static_cast<SLuint32>(frame.size() * sizeof(int16_t)));
}

// ── play ──────────────────────────────────────────────────────────────────────
bool AudioEngine::play(int track_index) {
    if (track_index < 0 || track_index >= track_count()) return false;

    stop();

    current_track_.store(track_index, std::memory_order_release);
    phase_.store(0.0f,  std::memory_order_release);
    sample_frames_.store(0, std::memory_order_release);

    if (!create_player()) return false;

    playing_.store(true, std::memory_order_release);

    // Pre-fill both buffers so playback starts immediately
    for (int i = 0; i < kNumBuffers; ++i) {
        auto& f = pcm_bufs_[i];
        fill_buffer(std::span<int16_t>(f.data(), f.size()));
        (*sl_queue_)->Enqueue(sl_queue_, f.data(),
                              static_cast<SLuint32>(f.size() * sizeof(int16_t)));
        active_buf_ = (active_buf_ + 1) % kNumBuffers;
    }

    (*sl_play_)->SetPlayState(sl_play_, SL_PLAYSTATE_PLAYING);
    LOGI("AudioEngine: playing track %d — %.*s @ %.1f Hz",
         track_index,
         (int)kTracks[track_index].name.size(), kTracks[track_index].name.data(),
         kTracks[track_index].fundamental_hz);
    return true;
}

// ── pause / resume / stop ─────────────────────────────────────────────────────
void AudioEngine::pause() {
    if (sl_play_ && playing_.load()) {
        (*sl_play_)->SetPlayState(sl_play_, SL_PLAYSTATE_PAUSED);
        playing_.store(false, std::memory_order_release);
        LOGI("AudioEngine: paused");
    }
}

void AudioEngine::resume() {
    if (sl_play_ && !playing_.load()) {
        playing_.store(true, std::memory_order_release);
        (*sl_play_)->SetPlayState(sl_play_, SL_PLAYSTATE_PLAYING);
        LOGI("AudioEngine: resumed");
    }
}

void AudioEngine::stop() {
    playing_.store(false, std::memory_order_release);
    if (sl_play_)  (*sl_play_)->SetPlayState(sl_play_, SL_PLAYSTATE_STOPPED);
    if (sl_queue_) (*sl_queue_)->Clear(sl_queue_);
    destroy_player();
    sample_frames_.store(0, std::memory_order_release);
    phase_.store(0.0f, std::memory_order_release);
    {
        std::lock_guard lock(spectrum_mtx_);
        spectrum_.fill(0.0f);
    }
}

void AudioEngine::next() {
    const int n = (current_track_.load() + 1) % track_count();
    play(n);
}

void AudioEngine::prev() {
    const int n = (current_track_.load() + track_count() - 1) % track_count();
    play(n);
}

// ── Queries ───────────────────────────────────────────────────────────────────
float AudioEngine::position_sec() const noexcept {
    return static_cast<float>(sample_frames_.load(std::memory_order_relaxed))
           / static_cast<float>(kSampleRate);
}

float AudioEngine::duration_sec() const noexcept {
    return kTracks[current_track_.load(std::memory_order_relaxed)].duration_sec;
}

const Track& AudioEngine::track_info(int index) const noexcept {
    const int i = testapp::clamp(index, 0, track_count() - 1);
    return kTracks[i];
}

std::array<float, kSpectrumBands> AudioEngine::spectrum_snapshot() const {
    std::lock_guard lock(spectrum_mtx_);
    return spectrum_;
}

} // namespace testapp
