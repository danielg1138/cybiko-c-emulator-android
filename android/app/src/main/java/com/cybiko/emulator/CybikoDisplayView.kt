package com.cybiko.emulator

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Rect
import android.graphics.RectF
import android.util.AttributeSet
import android.view.SurfaceHolder
import android.view.SurfaceView

class CybikoDisplayView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : SurfaceView(context, attrs, defStyleAttr), SurfaceHolder.Callback {

    private val frameBitmap = Bitmap.createBitmap(
        CybikoNative.LCD_WIDTH,
        CybikoNative.LCD_HEIGHT,
        Bitmap.Config.ARGB_8888
    )

    private val srcRect = Rect(0, 0, CybikoNative.LCD_WIDTH, CybikoNative.LCD_HEIGHT)
    private val dstRect = RectF()

    private val pixelPaint = Paint().apply {
        isFilterBitmap = false /* Nearest neighbor for crisp retro pixel art */
        isDither = false
    }

    private val bezelPaint = Paint().apply {
        color = Color.parseColor("#181A16")
        style = Paint.Style.FILL
    }

    private val borderPaint = Paint().apply {
        color = Color.parseColor("#383C32")
        style = Paint.Style.STROKE
        strokeWidth = 6f
    }

    private val textPaint = Paint().apply {
        color = Color.parseColor("#80A060")
        textSize = 28f
        isAntiAlias = true
    }

    var showTelemetry: Boolean = true

    init {
        holder.addCallback(this)
    }

    fun renderFrame(fps: Float, coreMs: Float) {
        if (!holder.surface.isValid) return

        CybikoNative.nativeRenderToBitmap(frameBitmap)

        val canvas: Canvas = holder.lockCanvas() ?: return
        try {
            drawCybikoScreen(canvas, fps, coreMs)
        } finally {
            holder.unlockCanvasAndPost(canvas)
        }
    }

    private fun drawCybikoScreen(canvas: Canvas, fps: Float, coreMs: Float) {
        val w = width.toFloat()
        val h = height.toFloat()

        /* Fill background / bezel area */
        canvas.drawRect(0f, 0f, w, h, bezelPaint)

        /* Calculate max integer scale that fits within display */
        val scaleX = (w * 0.90f) / CybikoNative.LCD_WIDTH
        val scaleY = (h * 0.90f) / CybikoNative.LCD_HEIGHT
        val scale = Math.max(1.0f, Math.min(scaleX, scaleY))

        val displayW = CybikoNative.LCD_WIDTH * scale
        val displayH = CybikoNative.LCD_HEIGHT * scale

        val left = (w - displayW) / 2f
        val top = (h - displayH) / 2f
        dstRect.set(left, top, left + displayW, top + displayH)

        /* Outer screen frame */
        canvas.drawRect(
            dstRect.left - 6f,
            dstRect.top - 6f,
            dstRect.right + 6f,
            dstRect.bottom + 6f,
            borderPaint
        )

        /* Draw scaled LCD pixels */
        canvas.drawBitmap(frameBitmap, srcRect, dstRect, pixelPaint)

        /* Optional Telemetry HUD (Rule 2) */
        if (showTelemetry) {
            val telemetryText = String.format("FPS: %.1f | Core: %.2f ms", fps, coreMs)
            canvas.drawText(telemetryText, 30f, 50f, textPaint)
        }
    }

    override fun surfaceCreated(holder: SurfaceHolder) {}
    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {}
    override fun surfaceDestroyed(holder: SurfaceHolder) {}
}
