// ─────────────────────────────────────────────────────────────────────────────
//  PlayerViewModel.kt  —  Player state (Kotlin, no AndroidX, no coroutines)
// ─────────────────────────────────────────────────────────────────────────────
package com.testapp

import android.os.Handler
import android.os.Looper
import android.util.Log

private const val TAG     = "TESTAPP"
private const val POLL_MS = 100L

data class PlayerState(
    val trackIndex:  Int     = 0,
    val trackName:   String  = "—",
    val trackArtist: String  = "—",
    val isPlaying:   Boolean = false,
    val positionSec: Float   = 0f,
    val durationSec: Float   = 60f,
    val trackCount:  Int     = 0,
)

typealias StateListener = (PlayerState) -> Unit

class PlayerViewModel {

    var handle: Long = 0L
        private set

    private var state         = PlayerState()
    private val listeners     = mutableListOf<StateListener>()
    private val mainHandler   = Handler(Looper.getMainLooper())
    private var pollingActive = false

    // ── Init / Destroy ────────────────────────────────────────────────────────
    fun create() {
        Thread {
            handle = MainActivity.nativeCreate()
            if (handle == 0L) { Log.e(TAG, "nativeCreate() returned null"); return@Thread }
            val count = MainActivity.nativeGetTrackCount(handle)
            Log.i(TAG, "Engine ready — $count tracks")
            post(state.copy(
                trackCount  = count,
                trackName   = MainActivity.nativeGetTrackName(handle, 0),
                trackArtist = MainActivity.nativeGetTrackArtist(handle, 0),
                durationSec = MainActivity.nativeGetTrackDuration(handle, 0),
            ))
        }.start()
    }

    fun destroy() {
        stopPolling()
        if (handle != 0L) {
            MainActivity.nativeStop(handle)
            MainActivity.nativeDestroy(handle)
            handle = 0L
        }
    }

    // ── Listeners ─────────────────────────────────────────────────────────────
    fun addListener(l: StateListener)    { listeners.add(l)    }
    fun removeListener(l: StateListener) { listeners.remove(l) }

    private fun post(s: PlayerState) {
        state = s
        mainHandler.post { listeners.forEach { it(s) } }
    }

    // ── Transport ─────────────────────────────────────────────────────────────
    fun play(trackIndex: Int = state.trackIndex) {
        if (handle == 0L) return
        Thread {
            if (MainActivity.nativePlay(handle, trackIndex)) {
                post(state.copy(
                    trackIndex  = trackIndex,
                    trackName   = MainActivity.nativeGetTrackName(handle, trackIndex),
                    trackArtist = MainActivity.nativeGetTrackArtist(handle, trackIndex),
                    durationSec = MainActivity.nativeGetTrackDuration(handle, trackIndex),
                    isPlaying   = true,
                    positionSec = 0f,
                ))
                startPolling()
            }
        }.start()
    }

    fun togglePlayPause() {
        if (handle == 0L) return
        Thread {
            if (state.isPlaying) {
                MainActivity.nativePause(handle)
                post(state.copy(isPlaying = false))
                stopPolling()
            } else {
                MainActivity.nativeResume(handle)
                post(state.copy(isPlaying = true))
                startPolling()
            }
        }.start()
    }

    fun next() {
        if (handle == 0L) return
        Thread {
            MainActivity.nativeNext(handle)
            val idx = MainActivity.nativeGetCurrentTrack(handle)
            post(state.copy(
                trackIndex  = idx,
                trackName   = MainActivity.nativeGetTrackName(handle, idx),
                trackArtist = MainActivity.nativeGetTrackArtist(handle, idx),
                durationSec = MainActivity.nativeGetTrackDuration(handle, idx),
                isPlaying   = true, positionSec = 0f,
            ))
            startPolling()
        }.start()
    }

    fun prev() {
        if (handle == 0L) return
        Thread {
            MainActivity.nativePrev(handle)
            val idx = MainActivity.nativeGetCurrentTrack(handle)
            post(state.copy(
                trackIndex  = idx,
                trackName   = MainActivity.nativeGetTrackName(handle, idx),
                trackArtist = MainActivity.nativeGetTrackArtist(handle, idx),
                durationSec = MainActivity.nativeGetTrackDuration(handle, idx),
                isPlaying   = true, positionSec = 0f,
            ))
            startPolling()
        }.start()
    }

    // ── Position polling ──────────────────────────────────────────────────────
    private val pollRunnable = object : Runnable {
        override fun run() {
            if (!pollingActive || handle == 0L) return
            if (state.isPlaying) {
                state = state.copy(positionSec = MainActivity.nativeGetPosition(handle))
                listeners.forEach { it(state) }
            }
            mainHandler.postDelayed(this, POLL_MS)
        }
    }

    private fun startPolling() {
        if (pollingActive) return
        pollingActive = true
        mainHandler.postDelayed(pollRunnable, POLL_MS)
    }

    private fun stopPolling() {
        pollingActive = false
        mainHandler.removeCallbacks(pollRunnable)
    }

    // ── GL surface callbacks (called from GL thread via VisualizerSurface) ────
    fun onSurfaceCreated() {
        if (handle != 0L) MainActivity.nativeSurfaceCreated(handle)
    }

    fun onSurfaceChanged(width: Int, height: Int) {
        if (handle != 0L) MainActivity.nativeSurfaceChanged(handle, width, height)
    }

    fun onDrawFrame() {
        if (handle != 0L) MainActivity.nativeDrawFrame(handle)
    }

    fun currentState(): PlayerState = state
}
