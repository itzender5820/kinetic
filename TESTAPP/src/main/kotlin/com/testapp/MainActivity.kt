// ─────────────────────────────────────────────────────────────────────────────
//  com.testapp.MainActivity  —  Music player UI (Kotlin, plain android.app.Activity)
//
//  No AndroidX. No Jetpack. No coroutines.
//  All UI built programmatically so no layout inflater / XML dependency.
//
//  Visual layout:
//    ┌──────────────────────────────────┐
//    │  ⚡ TESTAPP  Music Player         │  header
//    ├──────────────────────────────────┤
//    │  Track Name                      │  title (animated on change)
//    │  Artist — Kinetic Audio          │  artist
//    │──────────────────────────────────│  divider
//    │                                  │
//    │      OpenGL ES Visualizer        │  ~50 % height  (GLSurfaceView)
//    │  32-band radial spectrum + orb   │
//    │                                  │
//    │  ████████░░░  0:12 / 1:00        │  seekbar + times
//    │     ◀◀    ⏸    ▶▶               │  transport
//    │   [ 1 ] [ 2 ] [ 3 ] [ 4 ]       │  track selector
//    └──────────────────────────────────┘
// ─────────────────────────────────────────────────────────────────────────────
package com.testapp

import android.animation.ValueAnimator
import android.app.Activity
import android.graphics.Color
import android.graphics.Typeface
import android.os.Bundle
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.view.ViewGroup.LayoutParams.MATCH_PARENT
import android.view.ViewGroup.LayoutParams.WRAP_CONTENT
import android.widget.LinearLayout
import android.widget.SeekBar
import android.widget.TextView

class MainActivity : Activity() {

    // ── JNI ──────────────────────────────────────────────────────────────────
    companion object {
        init { System.loadLibrary("TESTAPP") }

        @JvmStatic external fun nativeCreate(): Long
        @JvmStatic external fun nativeDestroy(h: Long)
        @JvmStatic external fun nativePlay(h: Long, index: Int): Boolean
        @JvmStatic external fun nativePause(h: Long)
        @JvmStatic external fun nativeResume(h: Long)
        @JvmStatic external fun nativeStop(h: Long)
        @JvmStatic external fun nativeNext(h: Long)
        @JvmStatic external fun nativePrev(h: Long)
        @JvmStatic external fun nativeIsPlaying(h: Long): Boolean
        @JvmStatic external fun nativeGetCurrentTrack(h: Long): Int
        @JvmStatic external fun nativeGetPosition(h: Long): Float
        @JvmStatic external fun nativeGetDuration(h: Long): Float
        @JvmStatic external fun nativeGetTrackCount(h: Long): Int
        @JvmStatic external fun nativeGetTrackName(h: Long, i: Int): String
        @JvmStatic external fun nativeGetTrackArtist(h: Long, i: Int): String
        @JvmStatic external fun nativeGetTrackDuration(h: Long, i: Int): Float
        @JvmStatic external fun nativeGetSpectrum(h: Long): FloatArray
        @JvmStatic external fun nativeSurfaceCreated(h: Long)
        @JvmStatic external fun nativeSurfaceChanged(h: Long, w: Int, h2: Int)
        @JvmStatic external fun nativeDrawFrame(h: Long)
    }

    // ── State ─────────────────────────────────────────────────────────────────
    private val viewModel = PlayerViewModel()

    // ── Views ─────────────────────────────────────────────────────────────────
    private lateinit var surface:       VisualizerSurface
    private lateinit var tvTitle:       TextView
    private lateinit var tvArtist:      TextView
    private lateinit var tvPos:         TextView
    private lateinit var tvDur:         TextView
    private lateinit var seekBar:       SeekBar
    private lateinit var btnPlayPause:  TextView
    private val trackBtns             = mutableListOf<TextView>()

    // ── Palette ───────────────────────────────────────────────────────────────
    private val cBg      = Color.parseColor("#080810")
    private val cSurface = Color.parseColor("#12121E")
    private val cAccent  = Color.parseColor("#2277FF")
    private val cPink    = Color.parseColor("#FF2299")
    private val cText    = Color.parseColor("#E8E8FF")
    private val cDim     = Color.parseColor("#6666AA")
    private val cDiv     = Color.parseColor("#222244")

    private var pulseAnim: ValueAnimator? = null
    private var lastTrack = -1

    // ── onCreate ──────────────────────────────────────────────────────────────
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(buildUI())
        surface.setViewModel(viewModel)
        viewModel.addListener(::applyState)
        viewModel.create()
    }

    override fun onResume()  { super.onResume();  surface.onResume()  }
    override fun onPause()   { super.onPause();   surface.onPause()   }
    override fun onDestroy() { super.onDestroy(); viewModel.destroy() }

    // ── UI construction ───────────────────────────────────────────────────────
    private fun buildUI(): View {
        val root = vbox(MATCH_PARENT, MATCH_PARENT).apply {
            setBackgroundColor(cBg)
        }

        // ── Header ────────────────────────────────────────────────────────────
        val header = hbox(MATCH_PARENT, WRAP_CONTENT).apply {
            setBackgroundColor(cSurface)
            setPadding(dp(16), dp(14), dp(16), dp(14))
            gravity = Gravity.CENTER_VERTICAL
        }
        header.addView(tv("⚡", 20f, Color.WHITE, false).apply { setPadding(0,0,dp(6),0) })
        header.addView(tv("TESTAPP", 17f, cAccent, true).apply { letterSpacing = 0.12f })
        header.addView(tv("  Music Player", 12f, cDim, false))
        root.addView(header, lp(MATCH_PARENT, WRAP_CONTENT))

        // ── Track info ────────────────────────────────────────────────────────
        tvTitle = tv("—", 21f, cText, true).apply {
            gravity = Gravity.CENTER_HORIZONTAL
            setPadding(dp(16), dp(12), dp(16), dp(2))
        }
        tvArtist = tv("—", 13f, cDim, false).apply {
            gravity = Gravity.CENTER_HORIZONTAL
            setPadding(dp(16), 0, dp(16), dp(8))
        }
        root.addView(tvTitle,  lp(MATCH_PARENT, WRAP_CONTENT))
        root.addView(tvArtist, lp(MATCH_PARENT, WRAP_CONTENT))

        // divider
        root.addView(View(this).apply { setBackgroundColor(cDiv) },
            LinearLayout.LayoutParams(MATCH_PARENT, 1).apply {
                setMargins(dp(24), 0, dp(24), 0)
            })

        // ── Visualizer (weight=1 → fills remaining space) ────────────────────
        surface = VisualizerSurface(this)
        root.addView(surface, LinearLayout.LayoutParams(MATCH_PARENT, 0, 1f))

        // ── Progress ──────────────────────────────────────────────────────────
        val progCol = vbox(MATCH_PARENT, WRAP_CONTENT).apply {
            setPadding(dp(16), dp(8), dp(16), dp(4))
        }
        seekBar = SeekBar(this).apply {
            max = 1000
            progressDrawable?.setTint(cAccent)
            thumb?.setTint(cPink)
            // Seeking is display-only in this version
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(sb: SeekBar?, p: Int, user: Boolean) {}
                override fun onStartTrackingTouch(sb: SeekBar?) {}
                override fun onStopTrackingTouch(sb: SeekBar?) {}
            })
        }
        progCol.addView(seekBar, lp(MATCH_PARENT, WRAP_CONTENT))

        val timeRow = hbox(MATCH_PARENT, WRAP_CONTENT)
        tvPos = tv("0:00", 11f, cDim, false)
        tvDur = tv("1:00", 11f, cDim, false)
        timeRow.addView(tvPos, lp(WRAP_CONTENT, WRAP_CONTENT))
        timeRow.addView(View(this), LinearLayout.LayoutParams(0, 1, 1f))
        timeRow.addView(tvDur, lp(WRAP_CONTENT, WRAP_CONTENT))
        progCol.addView(timeRow, lp(MATCH_PARENT, WRAP_CONTENT))
        root.addView(progCol, lp(MATCH_PARENT, WRAP_CONTENT))

        // ── Transport ─────────────────────────────────────────────────────────
        val transport = hbox(MATCH_PARENT, WRAP_CONTENT).apply {
            gravity = Gravity.CENTER
            setPadding(dp(16), dp(4), dp(16), dp(4))
        }
        val btnPrev = ctrlBtn("◀◀", 22f) {
            PlayerControls.animateButtonTap(it)
            viewModel.prev()
        }
        btnPlayPause = ctrlBtn("▶", 34f) {
            PlayerControls.animateButtonTap(it)
            viewModel.togglePlayPause()
        }.apply { setTextColor(cAccent) }
        val btnNext = ctrlBtn("▶▶", 22f) {
            PlayerControls.animateButtonTap(it)
            viewModel.next()
        }

        transport.addView(btnPrev,     LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f).apply { gravity = Gravity.CENTER })
        transport.addView(btnPlayPause,LinearLayout.LayoutParams(0, WRAP_CONTENT, 1.6f).apply { gravity = Gravity.CENTER })
        transport.addView(btnNext,     LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f).apply { gravity = Gravity.CENTER })
        root.addView(transport, lp(MATCH_PARENT, WRAP_CONTENT))

        // ── Track selector ────────────────────────────────────────────────────
        val selector = hbox(MATCH_PARENT, WRAP_CONTENT).apply {
            gravity = Gravity.CENTER
            setPadding(dp(12), dp(6), dp(12), dp(16))
        }
        trackBtns.clear()
        for (i in 0 until 4) {
            val btn = tv("${i+1}", 13f, cDim, true).apply {
                gravity = Gravity.CENTER
                setBackgroundColor(cSurface)
                setPadding(dp(10), dp(10), dp(10), dp(10))
                setOnClickListener {
                    PlayerControls.animateButtonTap(this)
                    viewModel.play(i)
                }
            }
            trackBtns.add(btn)
            val blp = LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f).apply { setMargins(dp(3),0,dp(3),0) }
            selector.addView(btn, blp)
        }
        root.addView(selector, lp(MATCH_PARENT, WRAP_CONTENT))

        return root
    }

    // ── Apply state ───────────────────────────────────────────────────────────
    private fun applyState(s: PlayerState) {
        // Track change
        if (s.trackIndex != lastTrack) {
            lastTrack = s.trackIndex
            PlayerControls.animateTextChange(tvTitle,  s.trackName)
            PlayerControls.animateTextChange(tvArtist, s.trackArtist)
            trackBtns.forEachIndexed { i, btn ->
                val active = i == s.trackIndex
                btn.setTextColor(if (active) cAccent else cDim)
                btn.setBackgroundColor(if (active) Color.parseColor("#1A2244") else cSurface)
            }
        }

        // Play/pause icon
        btnPlayPause.text = if (s.isPlaying) "⏸" else "▶"

        // Progress bar + labels
        PlayerControls.updateProgress(seekBar, s.positionSec, s.durationSec)
        PlayerControls.updateTimeLabels(tvPos, tvDur, s.positionSec, s.durationSec)

        // Pulse animation
        if (s.isPlaying && pulseAnim?.isRunning != true) {
            pulseAnim = PlayerControls.createPulseAnimator(btnPlayPause).also { it.start() }
        } else if (!s.isPlaying) {
            pulseAnim?.cancel(); pulseAnim = null
            btnPlayPause.alpha = 1f
        }
    }

    // ── View factory helpers ──────────────────────────────────────────────────
    private fun dp(n: Int) = (n * resources.displayMetrics.density).toInt()

    private fun lp(w: Int, h: Int) = LinearLayout.LayoutParams(w, h)

    private fun vbox(w: Int, h: Int) = LinearLayout(this).apply {
        orientation = LinearLayout.VERTICAL
        layoutParams = ViewGroup.LayoutParams(w, h)
    }

    private fun hbox(w: Int, h: Int) = LinearLayout(this).apply {
        orientation = LinearLayout.HORIZONTAL
        layoutParams = ViewGroup.LayoutParams(w, h)
    }

    private fun tv(text: String, size: Float, color: Int, bold: Boolean) =
        TextView(this).apply {
            this.text = text
            textSize  = size
            setTextColor(color)
            typeface  = if (bold) Typeface.DEFAULT_BOLD else Typeface.DEFAULT
        }

    private fun ctrlBtn(label: String, size: Float, onClick: (View) -> Unit) =
        tv(label, size, cText, false).apply {
            gravity = Gravity.CENTER
            setOnClickListener { onClick(it) }
        }
}
