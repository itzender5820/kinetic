// ─────────────────────────────────────────────────────────────────────────────
//  com.testapp.VisualizerSurface  —  OpenGL ES 2.0 surface (Kotlin)
//  Uses plain android.opengl.GLSurfaceView — no AndroidX required.
// ─────────────────────────────────────────────────────────────────────────────
package com.testapp

import android.content.Context
import android.opengl.GLSurfaceView
import android.util.Log
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

private const val VTAG = "TESTAPP"

/**
 * GLSurfaceView that delegates all rendering to the C++ Renderer via JNI.
 * Must call setViewModel() before the surface is created.
 */
class VisualizerSurface(context: Context) : GLSurfaceView(context) {

    private var viewModel: PlayerViewModel? = null

    init {
        setEGLContextClientVersion(2)
        setRenderer(InnerRenderer())
        renderMode = RENDERMODE_CONTINUOUSLY
    }

    fun setViewModel(vm: PlayerViewModel) {
        viewModel = vm
    }

    private inner class InnerRenderer : Renderer {
        override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
            Log.d(VTAG, "GL surface created")
            viewModel?.onSurfaceCreated()
        }
        override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
            Log.d(VTAG, "GL surface changed ${width}x${height}")
            viewModel?.onSurfaceChanged(width, height)
        }
        override fun onDrawFrame(gl: GL10?) {
            viewModel?.onDrawFrame()
        }
    }
}
