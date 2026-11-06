package com.cybiko.emulator

import android.app.Presentation
import android.content.Context
import android.hardware.display.DisplayManager
import android.os.Bundle
import android.util.Log
import android.view.Display
import android.view.KeyEvent
import android.view.View
import android.view.WindowManager
import androidx.appcompat.app.AppCompatActivity
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.util.concurrent.atomic.AtomicBoolean

class MainActivity : AppCompatActivity() {

    private val TAG = "CybikoMainActivity"

    private lateinit var displayView: CybikoDisplayView
    private lateinit var keyboardView: CybikoKeyboardView

    private var keyboardPresentation: Presentation? = null

    private var emuThread: Thread? = null
    private val isRunning = AtomicBoolean(false)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        /* Keep screen on and hide system bars for immersive handheld experience */
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        window.decorView.systemUiVisibility = (
            View.SYSTEM_UI_FLAG_FULLSCREEN
                or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
        )

        setContentView(R.layout.activity_main)

        displayView = findViewById(R.id.displayView)
        keyboardView = findViewById(R.id.keyboardView)

        /* Initialize Native Core */
        CybikoNative.nativeInit(CybikoNative.PALETTE_CYBIKO_GREEN)

        /* Check for secondary display (Ayn Thor Clamshell Bottom Screen) */
        setupDualDisplayIfAvailable()

        /* Load ROMs and persistent NVRAM */
        loadFirmwareAndNvram()
    }

    private fun setupDualDisplayIfAvailable() {
        val dm = getSystemService(Context.DISPLAY_SERVICE) as DisplayManager
        val displays = dm.displays

        if (displays.size > 1) {
            /* Secondary display found (Thor bottom screen) */
            val secondaryDisplay: Display = displays[1]
            Log.i(TAG, "Secondary display detected: ${secondaryDisplay.name} (${secondaryDisplay.width}x${secondaryDisplay.height})")

            /* Hide on-screen keyboard from top display, expand display view */
            keyboardView.visibility = View.GONE

            /* Launch Presentation on secondary display */
            keyboardPresentation = object : Presentation(this, secondaryDisplay) {
                override fun onCreate(savedInstanceState: Bundle?) {
                    super.onCreate(savedInstanceState)
                    setContentView(CybikoKeyboardView(context))
                }
            }
            keyboardPresentation?.show()
        } else {
            Log.i(TAG, "Single display layout active (split top/bottom)")
        }
    }

    private fun loadFirmwareAndNvram() {
        val romDir = File(filesDir, "roms").apply { mkdirs() }
        val bootFile = File(romDir, "boot.bin")
        val flashFile = File(romDir, "flash.bin")
        val nvramFile = File(filesDir, "cybiko.nvram")

        /* Auto-extract bundled firmware from assets on first run */
        if (!bootFile.exists()) {
            try {
                assets.open("roms/boot.bin").use { input ->
                    FileOutputStream(bootFile).use { output -> input.copyTo(output) }
                }
                Log.i(TAG, "Extracted boot.bin from assets")
            } catch (e: Exception) {
                Log.w(TAG, "No boot.bin in assets", e)
            }
        }

        if (!flashFile.exists()) {
            try {
                assets.open("roms/flash.bin").use { input ->
                    FileOutputStream(flashFile).use { output -> input.copyTo(output) }
                }
                Log.i(TAG, "Extracted flash.bin from assets")
            } catch (e: Exception) {
                Log.w(TAG, "No flash.bin in assets", e)
            }
        }

        val bootBytes = if (bootFile.exists()) bootFile.readBytes() else null
        val flashBytes = if (flashFile.exists()) flashFile.readBytes() else null
        val nvramBytes = if (nvramFile.exists()) nvramFile.readBytes() else null

        if (bootBytes != null && flashBytes != null) {
            CybikoNative.nativeLoadRoms(bootBytes, flashBytes, nvramBytes)
            Log.i(TAG, "Firmware successfully loaded from ${romDir.absolutePath}")
        } else {
            Log.w(TAG, "Firmware files not found in ${romDir.absolutePath}. Place boot.bin & flash.bin there.")
        }
    }

    private fun saveNvram() {
        val nvramData = CybikoNative.nativeGetNvram() ?: return
        try {
            val nvramFile = File(filesDir, "cybiko.nvram")
            FileOutputStream(nvramFile).use { it.write(nvramData) }
            Log.i(TAG, "NVRAM successfully persisted (${nvramData.size} bytes)")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to save NVRAM", e)
        }
    }

    override fun onResume() {
        super.onResume()
        startEmulationLoop()
    }

    override fun onPause() {
        super.onPause()
        stopEmulationLoop()
        saveNvram()
    }

    override fun onDestroy() {
        super.onDestroy()
        keyboardPresentation?.dismiss()
        CybikoNative.nativeDestroy()
    }

    private fun startEmulationLoop() {
        if (isRunning.getAndSet(true)) return

        emuThread = Thread({
            Log.i(TAG, "Starting 60 FPS Cybiko emulation loop")
            while (isRunning.get()) {
                val frameStart = System.nanoTime()

                /* Run 1 core frame (~307,200 CPU cycles) */
                CybikoNative.nativeRunFrame()

                /* Blit frame to screen */
                displayView.renderFrame(
                    CybikoNative.nativeGetFps(),
                    CybikoNative.nativeGetCoreMs()
                )

                /* 60 FPS pacing (~16.6ms) */
                val elapsedMs = (System.nanoTime() - frameStart) / 1000000L
                if (elapsedMs < 16L) {
                    try {
                        Thread.sleep(16L - elapsedMs)
                    } catch (e: InterruptedException) {
                        break
                    }
                }
            }
            Log.i(TAG, "Cybiko emulation loop exited")
        }, "CybikoEmulatorThread").apply {
            priority = Thread.MAX_PRIORITY
            start()
        }
    }

    private fun stopEmulationLoop() {
        isRunning.set(false)
        emuThread?.interrupt()
        emuThread?.join(500)
        emuThread = null
    }

    /* -----------------------------------------------------------------------
     * Gamepad & Physical Controller Mapping (Ayn Thor controls)
     * ----------------------------------------------------------------------- */

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        val cyKey = ThorGamepadMapper.mapKeyEvent(event.keyCode)
        if (cyKey != 0) {
            if (event.repeatCount == 0) {
                val pressed = (event.action == KeyEvent.ACTION_DOWN)
                CybikoNative.nativeSendKey(cyKey, pressed)
            }
            return true
        }
        return super.dispatchKeyEvent(event)
    }
}
