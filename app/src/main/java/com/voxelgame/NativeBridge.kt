package com.voxelgame

import android.opengl.GLES30
import android.opengl.GLSurfaceView
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

object NativeBridge {
    init {
        System.loadLibrary("voxelgame")
    }

    external fun nativeInit()
    external fun nativeResize(width: Int, height: Int)
    external fun nativeFrame()
    external fun nativeToggleWireframe()
    external fun nativeStats(): String
}

class NativeRenderer(private val onInfo: (String) -> Unit) : GLSurfaceView.Renderer {
    private var frames = 0
    private var lastTime = System.nanoTime()
    private var infoText = ""

    override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
        val renderer = GLES30.glGetString(GLES30.GL_RENDERER)
        val version = GLES30.glGetString(GLES30.GL_VERSION)
        val extensions = GLES30.glGetString(GLES30.GL_EXTENSIONS) ?: ""
        val astc = extensions.contains("GL_KHR_texture_compression_astc_ldr")
        infoText = "GPU: $renderer\nGL: $version\nASTC: $astc"
        NativeBridge.nativeInit()
        onInfo(infoText + "\n" + NativeBridge.nativeStats())
    }

    override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
        NativeBridge.nativeResize(width, height)
    }

    override fun onDrawFrame(gl: GL10?) {
        NativeBridge.nativeFrame()
        frames++
        val now = System.nanoTime()
        if (now - lastTime >= 1_000_000_000L) {
            val fps = frames * 1_000_000_000.0 / (now - lastTime)
            onInfo(
                infoText + "\nFPS: " + String.format("%.1f", fps) +
                    "\n" + NativeBridge.nativeStats()
            )
            frames = 0
            lastTime = now
        }
    }
}
