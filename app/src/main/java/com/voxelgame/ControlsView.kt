package com.voxelgame

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.view.MotionEvent
import android.view.View
import kotlin.math.hypot
import kotlin.math.max
import kotlin.math.min

class ControlsView(context: Context) : View(context) {
    private val d = resources.displayMetrics.density
    private val stickRadius = 60f * d
    private val knobRadius = 26f * d
    private val jumpRadius = 44f * d
    private val deadZone = 0.12f
    private val degPerPixel = 0.12f

    private val prefs = context.getSharedPreferences("voxel_settings", Context.MODE_PRIVATE)
    private var sensitivity = prefs.getFloat("sensitivity", 1.0f)

    // Movement stick: appears wherever the left thumb lands.
    private var moveId = -1
    private var originX = 0f
    private var originY = 0f
    private var knobX = 0f
    private var knobY = 0f

    // Look: drag anywhere on the right side.
    private var lookId = -1
    private var lastX = 0f
    private var lastY = 0f

    private var jumpId = -1
    private var jumpX = 0f
    private var jumpY = 0f

    private val wireRect = RectF()
    private val sensDownRect = RectF()
    private val sensUpRect = RectF()

    private val ringPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 3f * d
        color = Color.argb(128, 255, 255, 255)
    }
    private val fillPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = Color.argb(51, 255, 255, 255)
    }
    private val activePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = Color.argb(110, 255, 255, 255)
    }
    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        textSize = 13f * d
        textAlign = Paint.Align.CENTER
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        val bw = 52f * d
        val bh = 38f * d
        val gap = 8f * d
        val top = 12f * d
        val right = w - 56f * d  // keeps clear of a camera cutout on the right
        wireRect.set(right - bw, top, right, top + bh)
        sensUpRect.set(wireRect.left - gap - bw, top, wireRect.left - gap, top + bh)
        sensDownRect.set(sensUpRect.left - gap - bw, top, sensUpRect.left - gap, top + bh)
        jumpX = w - 100f * d
        jumpY = h - 90f * d
    }

    override fun onDraw(canvas: Canvas) {
        if (moveId != -1) {
            canvas.drawCircle(originX, originY, stickRadius, ringPaint)
            canvas.drawCircle(knobX, knobY, knobRadius, activePaint)
        } else {
            canvas.drawCircle(width * 0.16f, height * 0.70f, stickRadius, ringPaint)
        }

        canvas.drawCircle(jumpX, jumpY, jumpRadius, if (jumpId != -1) activePaint else fillPaint)
        canvas.drawCircle(jumpX, jumpY, jumpRadius, ringPaint)
        canvas.drawText("JUMP", jumpX, jumpY + textPaint.textSize * 0.35f, textPaint)

        drawButton(canvas, wireRect, "WIRE")
        drawButton(canvas, sensDownRect, "S-")
        drawButton(canvas, sensUpRect, "S+")
        canvas.drawText(
            "Sens x" + String.format("%.2f", sensitivity),
            (sensDownRect.left + wireRect.right) / 2f,
            wireRect.bottom + 18f * d,
            textPaint
        )
    }

    private fun drawButton(canvas: Canvas, r: RectF, label: String) {
        canvas.drawRoundRect(r, 8f * d, 8f * d, fillPaint)
        canvas.drawRoundRect(r, 8f * d, 8f * d, ringPaint)
        canvas.drawText(label, r.centerX(), r.centerY() + textPaint.textSize * 0.35f, textPaint)
    }

    override fun onTouchEvent(e: MotionEvent): Boolean {
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                val i = e.actionIndex
                onDown(e.getPointerId(i), e.getX(i), e.getY(i))
            }
            MotionEvent.ACTION_MOVE -> {
                for (i in 0 until e.pointerCount) {
                    onMove(e.getPointerId(i), e.getX(i), e.getY(i))
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> {
                onUp(e.getPointerId(e.actionIndex))
            }
            MotionEvent.ACTION_CANCEL -> reset()
        }
        invalidate()
        return true
    }

    private fun onDown(id: Int, x: Float, y: Float) {
        if (wireRect.contains(x, y)) {
            NativeBridge.nativeToggleWireframe()
            return
        }
        if (sensDownRect.contains(x, y)) {
            setSensitivity(sensitivity / 1.25f)
            return
        }
        if (sensUpRect.contains(x, y)) {
            setSensitivity(sensitivity * 1.25f)
            return
        }
        if (jumpId == -1 && hypot(x - jumpX, y - jumpY) <= jumpRadius * 1.3f) {
            jumpId = id
            NativeBridge.nativeSetJump(true)
            return
        }
        if (x < width * 0.45f) {
            if (moveId == -1) {
                moveId = id
                originX = x
                originY = y
                knobX = x
                knobY = y
                NativeBridge.nativeSetMove(0f, 0f)
            }
        } else if (lookId == -1) {
            lookId = id
            lastX = x
            lastY = y
        }
    }

    private fun onMove(id: Int, x: Float, y: Float) {
        if (id == moveId) {
            val dx = x - originX
            val dy = y - originY
            val dist = hypot(dx, dy)
            val clamped = min(dist, stickRadius)
            if (dist > 0f) {
                knobX = originX + dx / dist * clamped
                knobY = originY + dy / dist * clamped
            }
            val mag = clamped / stickRadius
            if (dist == 0f || mag < deadZone) {
                NativeBridge.nativeSetMove(0f, 0f)
            } else {
                val scale = (mag - deadZone) / (1f - deadZone)
                // Screen up (negative y) means forward.
                NativeBridge.nativeSetMove(dx / dist * scale, -dy / dist * scale)
            }
        } else if (id == lookId) {
            val k = degPerPixel * sensitivity
            // Drag right turns right; drag up looks up.
            NativeBridge.nativeLook((x - lastX) * k, -(y - lastY) * k)
            lastX = x
            lastY = y
        }
    }

    private fun onUp(id: Int) {
        if (id == moveId) {
            moveId = -1
            NativeBridge.nativeSetMove(0f, 0f)
        }
        if (id == lookId) {
            lookId = -1
        }
        if (id == jumpId) {
            jumpId = -1
            NativeBridge.nativeSetJump(false)
        }
    }

    private fun setSensitivity(value: Float) {
        sensitivity = max(0.25f, min(4.0f, value))
        prefs.edit().putFloat("sensitivity", sensitivity).apply()
    }

    // Releases everything; called on cancel and when the app is paused.
    fun reset() {
        moveId = -1
        lookId = -1
        jumpId = -1
        NativeBridge.nativeSetMove(0f, 0f)
        NativeBridge.nativeSetJump(false)
        invalidate()
    }
}