package com.cybiko.emulator

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.util.AttributeSet
import android.view.HapticFeedbackConstants
import android.view.MotionEvent
import android.view.View

class CybikoKeyboardView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    data class KeyButton(
        val id: Int,
        val label: String,
        val subLabel: String? = null,
        val rect: RectF = RectF(),
        var isPressed: Boolean = false
    )

    private val keys = mutableListOf<KeyButton>()
    private val activeTouches = mutableMapOf<Int, KeyButton>() /* PointerId -> KeyButton */

    private val bgPaint = Paint().apply {
        color = Color.parseColor("#222620")
        style = Paint.Style.FILL
    }

    private val keyNormalPaint = Paint().apply {
        color = Color.parseColor("#343A30")
        style = Paint.Style.FILL
    }

    private val keyPressedPaint = Paint().apply {
        color = Color.parseColor("#608050")
        style = Paint.Style.FILL
    }

    private val keyBorderPaint = Paint().apply {
        color = Color.parseColor("#444C3E")
        style = Paint.Style.STROKE
        strokeWidth = 3f
    }

    private val textPaint = Paint().apply {
        color = Color.parseColor("#E0E8D0")
        textSize = 34f
        textAlign = Paint.Align.CENTER
        isAntiAlias = true
    }

    private val subTextPaint = Paint().apply {
        color = Color.parseColor("#A0C080")
        textSize = 22f
        textAlign = Paint.Align.CENTER
        isAntiAlias = true
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        rebuildKeyboardLayout(w.toFloat(), h.toFloat())
    }

    private fun rebuildKeyboardLayout(w: Float, h: Float) {
        keys.clear()

        val padding = 12f
        val rows = 5
        val rowHeight = (h - (padding * (rows + 1))) / rows

        /* Row 0: Soft keys & Quick System Actions */
        val row0 = listOf(
            KeyButton(CybikoNative.KEY_ESC, "ESC"),
            KeyButton(CybikoNative.KEY_F1, "F1"),
            KeyButton(CybikoNative.KEY_F2, "F2"),
            KeyButton(CybikoNative.KEY_F3, "F3"),
            KeyButton(CybikoNative.KEY_F4, "F4"),
            KeyButton(CybikoNative.KEY_F5, "F5"),
            KeyButton(CybikoNative.KEY_F6, "F6"),
            KeyButton(CybikoNative.KEY_F7, "F7"),
            KeyButton(CybikoNative.KEY_DEL, "DEL")
        )

        /* Row 1: Q - P with Numbers */
        val row1 = listOf(
            KeyButton(CybikoNative.KEY_Q, "Q", "1"),
            KeyButton(CybikoNative.KEY_W, "W", "2"),
            KeyButton(CybikoNative.KEY_E, "E", "3"),
            KeyButton(CybikoNative.KEY_R, "R", "4"),
            KeyButton(CybikoNative.KEY_T, "T", "5"),
            KeyButton(CybikoNative.KEY_Y, "Y", "6"),
            KeyButton(CybikoNative.KEY_U, "U", "7"),
            KeyButton(CybikoNative.KEY_I, "I", "8"),
            KeyButton(CybikoNative.KEY_O, "O", "9"),
            KeyButton(CybikoNative.KEY_P, "P", "0")
        )

        /* Row 2: A - L, Semicolon */
        val row2 = listOf(
            KeyButton(CybikoNative.KEY_TAB, "TAB"),
            KeyButton(CybikoNative.KEY_A, "A"),
            KeyButton(CybikoNative.KEY_S, "S"),
            KeyButton(CybikoNative.KEY_D, "D"),
            KeyButton(CybikoNative.KEY_F, "F"),
            KeyButton(CybikoNative.KEY_G, "G"),
            KeyButton(CybikoNative.KEY_H, "H"),
            KeyButton(CybikoNative.KEY_J, "J"),
            KeyButton(CybikoNative.KEY_K, "K"),
            KeyButton(CybikoNative.KEY_L, "L"),
            KeyButton(CybikoNative.KEY_SEMICOLON, ";")
        )

        /* Row 3: Shift, Z - M, Comma, Period, Enter */
        val row3 = listOf(
            KeyButton(CybikoNative.KEY_SHIFT, "SHIFT"),
            KeyButton(CybikoNative.KEY_Z, "Z"),
            KeyButton(CybikoNative.KEY_X, "X"),
            KeyButton(CybikoNative.KEY_C, "C"),
            KeyButton(CybikoNative.KEY_V, "V"),
            KeyButton(CybikoNative.KEY_B, "B"),
            KeyButton(CybikoNative.KEY_N, "N"),
            KeyButton(CybikoNative.KEY_M, "M"),
            KeyButton(CybikoNative.KEY_COMMA, ","),
            KeyButton(CybikoNative.KEY_PERIOD, "."),
            KeyButton(CybikoNative.KEY_ENTER, "ENTER")
        )

        /* Row 4: Fn, Select, Space, D-Pad navigation */
        val row4 = listOf(
            KeyButton(CybikoNative.KEY_FN, "Fn"),
            KeyButton(CybikoNative.KEY_SELECT, "SELECT"),
            KeyButton(CybikoNative.KEY_SPACE, "SPACE"),
            KeyButton(CybikoNative.KEY_LEFT, "<"),
            KeyButton(CybikoNative.KEY_UP, "^"),
            KeyButton(CybikoNative.KEY_DOWN, "v"),
            KeyButton(CybikoNative.KEY_RIGHT, ">")
        )

        val allRows = listOf(row0, row1, row2, row3, row4)

        for ((rowIndex, row) in allRows.withIndex()) {
            val top = padding + rowIndex * (rowHeight + padding)
            val bottom = top + rowHeight

            /* Calculate key widths proportionally */
            val colCount = row.size
            val keyWidth = (w - (padding * (colCount + 1))) / colCount

            for ((colIndex, key) in row.withIndex()) {
                val left = padding + colIndex * (keyWidth + padding)
                val right = left + keyWidth
                key.rect.set(left, top, right, bottom)
                keys.add(key)
            }
        }
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        canvas.drawRect(0f, 0f, width.toFloat(), height.toFloat(), bgPaint)

        for (k in keys) {
            val paint = if (k.isPressed) keyPressedPaint else keyNormalPaint
            canvas.drawRoundRect(k.rect, 10f, 10f, paint)
            canvas.drawRoundRect(k.rect, 10f, 10f, keyBorderPaint)

            val centerX = k.rect.centerX()
            if (k.subLabel != null) {
                val mainY = k.rect.centerY() + 8f
                val subY = k.rect.top + 26f
                canvas.drawText(k.label, centerX, mainY, textPaint)
                canvas.drawText(k.subLabel, centerX, subY, subTextPaint)
            } else {
                val mainY = k.rect.centerY() + 12f
                canvas.drawText(k.label, centerX, mainY, textPaint)
            }
        }
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val actionIndex = event.actionIndex
        val pointerId = event.getPointerId(actionIndex)

        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                val x = event.getX(actionIndex)
                val y = event.getY(actionIndex)
                val touchedKey = findKeyAt(x, y)
                if (touchedKey != null) {
                    activeTouches[pointerId] = touchedKey
                    touchedKey.isPressed = true
                    performHapticFeedback(HapticFeedbackConstants.KEYBOARD_TAP)
                    CybikoNative.nativeSendKey(touchedKey.id, true)
                    invalidate()
                }
            }

            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> {
                val releasedKey = activeTouches.remove(pointerId)
                if (releasedKey != null) {
                    releasedKey.isPressed = false
                    CybikoNative.nativeSendKey(releasedKey.id, false)
                    invalidate()
                }
            }

            MotionEvent.ACTION_CANCEL -> {
                for ((_, k) in activeTouches) {
                    k.isPressed = false
                    CybikoNative.nativeSendKey(k.id, false)
                }
                activeTouches.clear()
                invalidate()
            }
        }
        return true
    }

    private fun findKeyAt(x: Float, y: Float): KeyButton? {
        return keys.firstOrNull { it.rect.contains(x, y) }
    }
}
