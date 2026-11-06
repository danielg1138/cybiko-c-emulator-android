package com.cybiko.emulator

import android.view.KeyEvent

object ThorGamepadMapper {

    /**
     * Maps Android KeyEvent keycodes (gamepad buttons, D-pad, keyboard)
     * to Cybiko key IDs.
     */
    fun mapKeyEvent(keyCode: Int): Int {
        return when (keyCode) {
            /* D-Pad */
            KeyEvent.KEYCODE_DPAD_UP -> CybikoNative.KEY_UP
            KeyEvent.KEYCODE_DPAD_DOWN -> CybikoNative.KEY_DOWN
            KeyEvent.KEYCODE_DPAD_LEFT -> CybikoNative.KEY_LEFT
            KeyEvent.KEYCODE_DPAD_RIGHT -> CybikoNative.KEY_RIGHT

            /* Ayn Thor Face Buttons */
            KeyEvent.KEYCODE_BUTTON_A -> CybikoNative.KEY_ENTER       /* A -> Enter */
            KeyEvent.KEYCODE_BUTTON_B -> CybikoNative.KEY_DEL         /* B -> Delete / Back */
            KeyEvent.KEYCODE_BUTTON_X -> CybikoNative.KEY_SELECT      /* X -> Select (Home) */
            KeyEvent.KEYCODE_BUTTON_Y -> CybikoNative.KEY_ESC         /* Y -> Escape */

            /* Shoulder / Trigger Buttons */
            KeyEvent.KEYCODE_BUTTON_L1 -> CybikoNative.KEY_F1         /* L1 -> Softkey 1 */
            KeyEvent.KEYCODE_BUTTON_R1 -> CybikoNative.KEY_F2         /* R1 -> Softkey 2 */
            KeyEvent.KEYCODE_BUTTON_L2 -> CybikoNative.KEY_F3         /* L2 -> Softkey 3 */
            KeyEvent.KEYCODE_BUTTON_R2 -> CybikoNative.KEY_F4         /* R2 -> Softkey 4 */
            KeyEvent.KEYCODE_BUTTON_THUMBL -> CybikoNative.KEY_FN     /* Left Stick Click -> Fn */
            KeyEvent.KEYCODE_BUTTON_THUMBR -> CybikoNative.KEY_SHIFT  /* Right Stick Click -> Shift */

            /* Start / Select Buttons & System Keys */
            KeyEvent.KEYCODE_BUTTON_START -> CybikoNative.KEY_ENTER
            KeyEvent.KEYCODE_BUTTON_SELECT -> CybikoNative.KEY_SELECT
            KeyEvent.KEYCODE_BACK -> CybikoNative.KEY_ESC            /* Hardware Back -> Cybiko ESC */

            /* Standard Hardware Keyboard fallback (if USB/Bluetooth keyboard attached) */
            KeyEvent.KEYCODE_A -> CybikoNative.KEY_A
            KeyEvent.KEYCODE_B -> CybikoNative.KEY_B
            KeyEvent.KEYCODE_C -> CybikoNative.KEY_C
            KeyEvent.KEYCODE_D -> CybikoNative.KEY_D
            KeyEvent.KEYCODE_E -> CybikoNative.KEY_E
            KeyEvent.KEYCODE_F -> CybikoNative.KEY_F
            KeyEvent.KEYCODE_G -> CybikoNative.KEY_G
            KeyEvent.KEYCODE_H -> CybikoNative.KEY_H
            KeyEvent.KEYCODE_I -> CybikoNative.KEY_I
            KeyEvent.KEYCODE_J -> CybikoNative.KEY_J
            KeyEvent.KEYCODE_K -> CybikoNative.KEY_K
            KeyEvent.KEYCODE_L -> CybikoNative.KEY_L
            KeyEvent.KEYCODE_M -> CybikoNative.KEY_M
            KeyEvent.KEYCODE_N -> CybikoNative.KEY_N
            KeyEvent.KEYCODE_O -> CybikoNative.KEY_O
            KeyEvent.KEYCODE_P -> CybikoNative.KEY_P
            KeyEvent.KEYCODE_Q -> CybikoNative.KEY_Q
            KeyEvent.KEYCODE_R -> CybikoNative.KEY_R
            KeyEvent.KEYCODE_S -> CybikoNative.KEY_S
            KeyEvent.KEYCODE_T -> CybikoNative.KEY_T
            KeyEvent.KEYCODE_U -> CybikoNative.KEY_U
            KeyEvent.KEYCODE_V -> CybikoNative.KEY_V
            KeyEvent.KEYCODE_W -> CybikoNative.KEY_W
            KeyEvent.KEYCODE_X -> CybikoNative.KEY_X
            KeyEvent.KEYCODE_Y -> CybikoNative.KEY_Y
            KeyEvent.KEYCODE_Z -> CybikoNative.KEY_Z

            KeyEvent.KEYCODE_0 -> CybikoNative.KEY_0
            KeyEvent.KEYCODE_1 -> CybikoNative.KEY_1
            KeyEvent.KEYCODE_2 -> CybikoNative.KEY_2
            KeyEvent.KEYCODE_3 -> CybikoNative.KEY_3
            KeyEvent.KEYCODE_4 -> CybikoNative.KEY_4
            KeyEvent.KEYCODE_5 -> CybikoNative.KEY_5
            KeyEvent.KEYCODE_6 -> CybikoNative.KEY_6
            KeyEvent.KEYCODE_7 -> CybikoNative.KEY_7
            KeyEvent.KEYCODE_8 -> CybikoNative.KEY_8
            KeyEvent.KEYCODE_9 -> CybikoNative.KEY_9

            KeyEvent.KEYCODE_ENTER -> CybikoNative.KEY_ENTER
            KeyEvent.KEYCODE_SPACE -> CybikoNative.KEY_SPACE
            KeyEvent.KEYCODE_DEL -> CybikoNative.KEY_DEL
            KeyEvent.KEYCODE_TAB -> CybikoNative.KEY_TAB
            KeyEvent.KEYCODE_ESCAPE -> CybikoNative.KEY_ESC
            KeyEvent.KEYCODE_PERIOD -> CybikoNative.KEY_PERIOD
            KeyEvent.KEYCODE_SEMICOLON -> CybikoNative.KEY_SEMICOLON
            KeyEvent.KEYCODE_COMMA -> CybikoNative.KEY_COMMA
            KeyEvent.KEYCODE_SHIFT_LEFT, KeyEvent.KEYCODE_SHIFT_RIGHT -> CybikoNative.KEY_SHIFT
            KeyEvent.KEYCODE_CTRL_LEFT, KeyEvent.KEYCODE_CTRL_RIGHT -> CybikoNative.KEY_FN

            else -> 0
        }
    }
}
