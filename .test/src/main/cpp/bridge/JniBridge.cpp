// ─────────────────────────────────────────────────────────────────────────────
//  bridge/JniBridge.cpp — JNI bridge
//
//  RACE CONDITION FIX:
//    nativeCreate() runs on a background Thread (Kotlin side).
//    The GL thread calls onSurfaceCreated() BEFORE the handle is set,
//    so ctx(0L) returns null and the renderer never gets initialized.
//    Next draw call uses prog_bg_=0, glGetAttribLocation returns -1,
//    glEnableVertexAttribArray(-1) → SIGSEGV on GPU driver.
//
//  Fix: AppContext stores surface dimensions and a "needs_gl_init" flag.
//    nativeDrawFrame checks this flag and calls on_surface_created() +
//    on_surface_changed() from the GL thread (EGL context is already current)
//    before any draw calls. This is safe because the GL thread owns the context.
// ─────────────────────────────────────────────────────────────────────────────
#include "common.hpp"
#include "AudioEngine.hpp"
#include "Renderer.hpp"
#include <jni.h>
#include <memory>
#include <atomic>

namespace testapp {

struct AppContext {
    std::unique_ptr<AudioEngine> audio;
    std::unique_ptr<Renderer>    renderer;
    float start_time_sec = 0.f;

    // ── GL init state ─────────────────────────────────────────────────────────
    // Tracks whether on_surface_created() has been called on the GL thread.
    // The Kotlin side may call nativeSurfaceCreated/Changed before nativeCreate()
    // completes (race), so we record the pending dimensions and re-initialize
    // at the top of the first nativeDrawFrame call.
    std::atomic<bool> gl_initialized  {false};
    std::atomic<bool> surface_ready   {false};  // true after nativeSurfaceChanged
    int  pending_w = 0;
    int  pending_h = 0;
};

static AppContext* ctx(jlong h) {
    return reinterpret_cast<AppContext*>(static_cast<uintptr_t>(h));
}

static float now_sec() {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<float>(ts.tv_sec) + static_cast<float>(ts.tv_nsec) * 1e-9f;
}

} // namespace testapp

#define JFUN(name) Java_com_testapp_MainActivity_##name
extern "C" {

// ── Lifecycle ──────────────────────────────────────────────────────────────────

JNIEXPORT jlong JNICALL
JFUN(nativeCreate)(JNIEnv*, jobject) {
    auto* c = new testapp::AppContext();
    c->audio    = std::make_unique<testapp::AudioEngine>();
    c->renderer = std::make_unique<testapp::Renderer>();
    c->start_time_sec = testapp::now_sec();

    if (!c->audio->init()) {
        LOGE("JNI: AudioEngine::init() failed");
        delete c;
        return 0L;
    }
    LOGI("JNI: AppContext created @ %p", static_cast<void*>(c));
    return static_cast<jlong>(reinterpret_cast<uintptr_t>(c));
}

JNIEXPORT void JNICALL
JFUN(nativeDestroy)(JNIEnv*, jobject, jlong handle) {
    auto* c = testapp::ctx(handle);
    if (!c) return;
    c->audio->shutdown();
    delete c;
    LOGI("JNI: AppContext destroyed");
}

// ── Playback control ───────────────────────────────────────────────────────────

JNIEXPORT jboolean JNICALL
JFUN(nativePlay)(JNIEnv*, jobject, jlong handle, jint index) {
    auto* c = testapp::ctx(handle);
    if (!c) return JNI_FALSE;
    c->start_time_sec = testapp::now_sec();
    return c->audio->play(static_cast<int>(index)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
JFUN(nativePause)(JNIEnv*, jobject, jlong handle) {
    if (auto* c = testapp::ctx(handle)) c->audio->pause();
}

JNIEXPORT void JNICALL
JFUN(nativeResume)(JNIEnv*, jobject, jlong handle) {
    if (auto* c = testapp::ctx(handle)) c->audio->resume();
}

JNIEXPORT void JNICALL
JFUN(nativeStop)(JNIEnv*, jobject, jlong handle) {
    if (auto* c = testapp::ctx(handle)) c->audio->stop();
}

JNIEXPORT void JNICALL
JFUN(nativeNext)(JNIEnv*, jobject, jlong handle) {
    if (auto* c = testapp::ctx(handle)) {
        c->start_time_sec = testapp::now_sec();
        c->audio->next();
    }
}

JNIEXPORT void JNICALL
JFUN(nativePrev)(JNIEnv*, jobject, jlong handle) {
    if (auto* c = testapp::ctx(handle)) {
        c->start_time_sec = testapp::now_sec();
        c->audio->prev();
    }
}

// ── State queries ──────────────────────────────────────────────────────────────

JNIEXPORT jboolean JNICALL
JFUN(nativeIsPlaying)(JNIEnv*, jobject, jlong handle) {
    auto* c = testapp::ctx(handle);
    return (c && c->audio->is_playing()) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
JFUN(nativeGetCurrentTrack)(JNIEnv*, jobject, jlong handle) {
    auto* c = testapp::ctx(handle);
    return c ? static_cast<jint>(c->audio->current_track()) : 0;
}

JNIEXPORT jfloat JNICALL
JFUN(nativeGetPosition)(JNIEnv*, jobject, jlong handle) {
    auto* c = testapp::ctx(handle);
    return c ? static_cast<jfloat>(c->audio->position_sec()) : 0.f;
}

JNIEXPORT jfloat JNICALL
JFUN(nativeGetDuration)(JNIEnv*, jobject, jlong handle) {
    auto* c = testapp::ctx(handle);
    return c ? static_cast<jfloat>(c->audio->duration_sec()) : 1.f;
}

JNIEXPORT jint JNICALL
JFUN(nativeGetTrackCount)(JNIEnv*, jobject, jlong handle) {
    auto* c = testapp::ctx(handle);
    return c ? static_cast<jint>(c->audio->track_count()) : 0;
}

JNIEXPORT jstring JNICALL
JFUN(nativeGetTrackName)(JNIEnv* env, jobject, jlong handle, jint index) {
    auto* c = testapp::ctx(handle);
    if (!c) return env->NewStringUTF("");
    return env->NewStringUTF(std::string(c->audio->track_info(index).name).c_str());
}

JNIEXPORT jstring JNICALL
JFUN(nativeGetTrackArtist)(JNIEnv* env, jobject, jlong handle, jint index) {
    auto* c = testapp::ctx(handle);
    if (!c) return env->NewStringUTF("");
    return env->NewStringUTF(std::string(c->audio->track_info(index).artist).c_str());
}

JNIEXPORT jfloat JNICALL
JFUN(nativeGetTrackDuration)(JNIEnv*, jobject, jlong handle, jint index) {
    auto* c = testapp::ctx(handle);
    return c ? static_cast<jfloat>(c->audio->track_info(index).duration_sec) : 60.f;
}

JNIEXPORT jfloatArray JNICALL
JFUN(nativeGetSpectrum)(JNIEnv* env, jobject, jlong handle) {
    jfloatArray arr = env->NewFloatArray(testapp::kSpectrumBands);
    if (!arr) return nullptr;
    if (auto* c = testapp::ctx(handle)) {
        auto spec = c->audio->spectrum_snapshot();
        env->SetFloatArrayRegion(arr, 0, testapp::kSpectrumBands, spec.data());
    }
    return arr;
}

// ── GL surface callbacks ──────────────────────────────────────────────────────
//
// These may be called BEFORE nativeCreate() completes (handle == 0).
// We record the surface dimensions in a global so the first valid draw
// frame can initialize the renderer on the GL thread.
//
// Thread safety: surface callbacks are always on the GL thread.
// handle is written once on the background thread (nativeCreate) and read
// on GL thread. The atomic flags ensure visibility.

// Fallback storage for when surface callbacks arrive before handle is set.
static int  g_pending_w      = 0;
static int  g_pending_h      = 0;
static bool g_surface_ready  = false;

JNIEXPORT void JNICALL
JFUN(nativeSurfaceCreated)(JNIEnv*, jobject, jlong handle) {
    auto* c = testapp::ctx(handle);
    if (!c) {
        // Handle not ready yet — record that surface was created.
        // nativeDrawFrame will initialize the renderer once handle is valid.
        LOGD("JNI: nativeSurfaceCreated called before handle ready — deferred");
        return;
    }
    if (!c->gl_initialized.load()) {
        c->renderer->on_surface_created();
        c->gl_initialized.store(true);
        LOGI("JNI: Renderer GL initialized (surface created)");
    }
}

JNIEXPORT void JNICALL
JFUN(nativeSurfaceChanged)(JNIEnv*, jobject, jlong handle, jint w, jint h) {
    g_pending_w     = static_cast<int>(w);
    g_pending_h     = static_cast<int>(h);
    g_surface_ready = true;

    auto* c = testapp::ctx(handle);
    if (!c) {
        LOGD("JNI: nativeSurfaceChanged %dx%d — handle not ready, stored pending dims", w, h);
        return;
    }
    if (c->gl_initialized.load()) {
        c->renderer->on_surface_changed(static_cast<int>(w), static_cast<int>(h));
    }
    c->pending_w = static_cast<int>(w);
    c->pending_h = static_cast<int>(h);
    c->surface_ready.store(true);
}

JNIEXPORT void JNICALL
JFUN(nativeDrawFrame)(JNIEnv*, jobject, jlong handle) {
    auto* c = testapp::ctx(handle);
    if (!c) return;

    // ── Deferred GL init (race condition fix) ─────────────────────────────────
    // If the GL surface was created before nativeCreate() finished, the renderer
    // was never initialized. We detect this here on the GL thread (EGL context
    // is already current) and initialize now, before any draw call.
    if (!c->gl_initialized.load()) {
        if (!c->renderer->on_surface_created()) {
            LOGE("JNI: Deferred on_surface_created() failed — skipping frame");
            return;
        }
        c->gl_initialized.store(true);
        LOGI("JNI: Renderer GL initialized (deferred from draw frame)");

        // Apply pending surface dimensions
        int w = c->pending_w > 0 ? c->pending_w : g_pending_w;
        int h = c->pending_h > 0 ? c->pending_h : g_pending_h;
        if (w > 0 && h > 0) {
            c->renderer->on_surface_changed(w, h);
        }
    }

    const float time_sec = testapp::now_sec() - c->start_time_sec;
    const float pos      = c->audio->position_sec();
    const float dur      = c->audio->duration_sec();
    const float pos_norm = (dur > 0.f) ? (pos / dur) : 0.f;
    const bool  playing  = c->audio->is_playing();
    const auto  spectrum = c->audio->spectrum_snapshot();

    c->renderer->draw_frame(spectrum, pos_norm, playing, time_sec);
}

} // extern "C"
