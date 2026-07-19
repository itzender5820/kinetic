// ─────────────────────────────────────────────────────────────────────────────
//  PlayerControls.kt  —  UI animation helpers (ported from Java to Kotlin)
//  Kotlin-native version eliminates the Java→Kotlin classpath resolution issue.
// ─────────────────────────────────────────────────────────────────────────────
package com.testapp

import android.animation.AnimatorSet
import android.animation.ObjectAnimator
import android.animation.ValueAnimator
import android.view.View
import android.view.animation.AccelerateDecelerateInterpolator
import android.view.animation.DecelerateInterpolator
import android.widget.SeekBar
import android.widget.TextView

object PlayerControls {

    @JvmStatic
    fun animateButtonTap(view: View?) {
        view ?: return
        val sx = ObjectAnimator.ofFloat(view, "scaleX", 1f, 1.25f, 1f)
        val sy = ObjectAnimator.ofFloat(view, "scaleY", 1f, 1.25f, 1f)
        AnimatorSet().apply {
            playTogether(sx, sy)
            duration = 220
            interpolator = AccelerateDecelerateInterpolator()
            start()
        }
    }

    @JvmStatic
    fun animateTextChange(view: TextView?, newText: String) {
        view ?: return
        view.animate()
            .scaleX(0.85f).scaleY(0.85f)
            .setDuration(100)
            .withEndAction {
                view.text = newText
                view.animate()
                    .scaleX(1f).scaleY(1f)
                    .setDuration(150)
                    .setInterpolator(AccelerateDecelerateInterpolator())
                    .start()
            }.start()
    }

    @JvmStatic
    fun updateProgress(bar: SeekBar?, positionSec: Float, durationSec: Float) {
        bar ?: return
        val max = bar.max
        val value = if (durationSec > 0f) ((positionSec / durationSec) * max).toInt() else 0
        bar.progress = value.coerceIn(0, max)
    }

    @JvmStatic
    fun updateTimeLabels(
        pos: TextView?, dur: TextView?,
        positionSec: Float, durationSec: Float
    ) {
        pos?.text = TrackInfo.formatTime(positionSec)
        dur?.text = TrackInfo.formatTime(durationSec)
    }

    @JvmStatic
    fun createPulseAnimator(view: View?): ValueAnimator =
        ValueAnimator.ofFloat(0.5f, 1.0f).apply {
            duration = 800
            repeatMode = ValueAnimator.REVERSE
            repeatCount = ValueAnimator.INFINITE
            addUpdateListener { view?.alpha = it.animatedValue as Float }
        }

    @JvmStatic
    fun fadeIn(view: View?, durationMs: Long) {
        view ?: return
        view.alpha = 0f
        view.visibility = View.VISIBLE
        view.animate().alpha(1f).setDuration(durationMs)
            .setInterpolator(DecelerateInterpolator()).start()
    }

    @JvmStatic
    fun fadeOut(view: View?, durationMs: Long) {
        view ?: return
        view.animate().alpha(0f).setDuration(durationMs)
            .setInterpolator(DecelerateInterpolator())
            .withEndAction { view.visibility = View.GONE }.start()
    }
}
