package com.cybiko.emulator

import android.graphics.Bitmap

object CybikoNative {
    init {
        System.loadLibrary("cybiko")
    }

    const val PALETTE_CYBIKO_GREEN = 0
    const val PALETTE_GRAYSCALE = 1
    const val PALETTE_AMBER = 2

    /* Speed Modes matching cybiko_speed_mode_t */
    const val SPEED_AUTHENTIC = 0  /* 1.0x Authentic H8S (CPI = 4) */
    const val SPEED_FAST = 1       /* 2.0x Fast Forward (CPI = 2) */
    const val SPEED_TURBO = 2      /* 4.0x Turbo Uncapped (CPI = 1) */
    const val SPEED_CLASSIC = 3    /* 0.67x Cybiko Classic 11 MHz (CPI = 6) */

    const val LCD_WIDTH = 160
    const val LCD_HEIGHT = 100

    /* Key constants matching cybiko_input.h */
    const val KEY_A = 1
    const val KEY_B = 2
    const val KEY_C = 3
    const val KEY_D = 4
    const val KEY_E = 5
    const val KEY_F = 6
    const val KEY_G = 7
    const val KEY_H = 8
    const val KEY_I = 9
    const val KEY_J = 10
    const val KEY_K = 11
    const val KEY_L = 12
    const val KEY_M = 13
    const val KEY_N = 14
    const val KEY_O = 15
    const val KEY_P = 16
    const val KEY_Q = 17
    const val KEY_R = 18
    const val KEY_S = 19
    const val KEY_T = 20
    const val KEY_U = 21
    const val KEY_V = 22
    const val KEY_W = 23
    const val KEY_X = 24
    const val KEY_Y = 25
    const val KEY_Z = 26

    const val KEY_0 = 27
    const val KEY_1 = 28
    const val KEY_2 = 29
    const val KEY_3 = 30
    const val KEY_4 = 31
    const val KEY_5 = 32
    const val KEY_6 = 33
    const val KEY_7 = 34
    const val KEY_8 = 35
    const val KEY_9 = 36

    const val KEY_UP = 37
    const val KEY_DOWN = 38
    const val KEY_LEFT = 39
    const val KEY_RIGHT = 40

    const val KEY_SELECT = 41
    const val KEY_ESC = 42
    const val KEY_DEL = 43
    const val KEY_ENTER = 44
    const val KEY_SPACE = 45
    const val KEY_TAB = 46

    const val KEY_AS = 47
    const val KEY_HELP = 48
    const val KEY_PERIOD = 49
    const val KEY_SEMICOLON = 50
    const val KEY_COMMA = 51

    const val KEY_FN = 52
    const val KEY_SHIFT = 53

    const val KEY_F1 = 54
    const val KEY_F2 = 55
    const val KEY_F3 = 56
    const val KEY_F4 = 57
    const val KEY_F5 = 58
    const val KEY_F6 = 59
    const val KEY_F7 = 60

    @JvmStatic external fun nativeInit(paletteMode: Int): Boolean
    @JvmStatic external fun nativeDestroy()
    @JvmStatic external fun nativeLoadRoms(bootRom: ByteArray?, flashRom: ByteArray?, nvram: ByteArray?): Boolean
    @JvmStatic external fun nativeInjectApp(name: String, appData: ByteArray): Boolean
    @JvmStatic external fun nativeReset()
    @JvmStatic external fun nativeRunFrame()
    @JvmStatic external fun nativeSendKey(keyId: Int, pressed: Boolean)
    @JvmStatic external fun nativeSendRawKey(col: Int, mask: Int, pressed: Boolean)
    @JvmStatic external fun nativeGetNvram(): ByteArray?
    @JvmStatic external fun nativeRenderToBitmap(bitmap: Bitmap): Boolean
    @JvmStatic external fun nativeSetPalette(paletteMode: Int)
    @JvmStatic external fun nativeGetFps(): Float
    @JvmStatic external fun nativeGetCoreMs(): Float
    @JvmStatic external fun nativeSetSpeedMode(mode: Int)
    @JvmStatic external fun nativeGetSpeedMode(): Int
}
