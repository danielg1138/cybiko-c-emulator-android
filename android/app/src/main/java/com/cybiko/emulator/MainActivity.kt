package com.cybiko.emulator

import android.app.Presentation
import android.content.Context
import android.hardware.display.DisplayManager
import android.net.Uri
import android.os.Bundle
import android.provider.OpenableColumns
import android.util.Log
import android.view.Display
import android.view.KeyEvent
import android.view.View
import android.view.WindowManager
import android.widget.Button
import android.widget.Toast
import androidx.activity.OnBackPressedCallback
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import java.io.File
import java.io.FileOutputStream
import java.util.concurrent.atomic.AtomicBoolean

class MainActivity : AppCompatActivity() {

    private val TAG = "CybikoMainActivity"

    private lateinit var displayView: CybikoDisplayView
    private lateinit var bottomContainer: View

    private var keyboardPresentation: Presentation? = null

    private var emuThread: Thread? = null
    private val isRunning = AtomicBoolean(false)
    private var currentPalette = CybikoNative.PALETTE_CYBIKO_GREEN

    private val openAppLauncher = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri: Uri? ->
        if (uri != null) {
            installAppFromUri(uri)
        }
    }

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
        bottomContainer = findViewById(R.id.bottomContainer)

        setupSingleScreenToolbar()

        /* Intercept system back button to route to Cybiko ESC key */
        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                CybikoNative.nativeSendKey(CybikoNative.KEY_ESC, true)
            }
        })

        /* Initialize Native Core */
        CybikoNative.nativeInit(currentPalette)

        /* Check for secondary display (Ayn Thor Clamshell Bottom Screen) */
        setupDualDisplayIfAvailable()

        /* Load ROMs and persistent NVRAM */
        loadFirmwareAndNvram()
    }

    private fun setupSingleScreenToolbar() {
        findViewById<Button>(R.id.btnMainLoadApp).setOnClickListener {
            openAppLauncher.launch(arrayOf("*/*"))
        }
        findViewById<Button>(R.id.btnMainSaveNvram).setOnClickListener {
            saveNvram()
            Toast.makeText(this, "NVRAM saved", Toast.LENGTH_SHORT).show()
        }
        findViewById<Button>(R.id.btnMainPalette).setOnClickListener {
            cyclePalette()
        }
        findViewById<Button>(R.id.btnMainReset).setOnClickListener {
            showResetDialog()
        }
    }

    private fun setupDualDisplayIfAvailable() {
        val dm = getSystemService(Context.DISPLAY_SERVICE) as DisplayManager
        val presentationDisplays = dm.getDisplays(DisplayManager.DISPLAY_CATEGORY_PRESENTATION)
        val secondaryDisplay: Display? = if (presentationDisplays.isNotEmpty()) {
            presentationDisplays[0]
        } else {
            val all = dm.displays
            all.firstOrNull { it.displayId != Display.DEFAULT_DISPLAY }
        }

        if (secondaryDisplay != null) {
            /* Secondary display found (Thor bottom screen) */
            Log.i(TAG, "Secondary presentation display detected: ${secondaryDisplay.name} (id=${secondaryDisplay.displayId}, ${secondaryDisplay.width}x${secondaryDisplay.height})")

            /* Hide bottom container from top display, leaving top screen purely for LCD */
            bottomContainer.visibility = View.GONE

            /* Launch Presentation on secondary display */
            keyboardPresentation = object : Presentation(this, secondaryDisplay) {
                override fun onCreate(savedInstanceState: Bundle?) {
                    super.onCreate(savedInstanceState)
                    setCancelable(false)
                    setCanceledOnTouchOutside(false)

                    window?.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN)
                    window?.decorView?.systemUiVisibility = (
                        View.SYSTEM_UI_FLAG_FULLSCREEN
                            or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                            or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                    )

                    val presentationView = layoutInflater.inflate(R.layout.presentation_bottom, null)
                    setContentView(presentationView)

                    presentationView.findViewById<Button>(R.id.btnLoadApp).setOnClickListener {
                        Log.i(TAG, "User tapped '+ LOAD .APP' button")
                        openAppLauncher.launch(arrayOf("*/*"))
                    }
                    presentationView.findViewById<Button>(R.id.btnSaveNvram).setOnClickListener {
                        Log.i(TAG, "User tapped 'SAVE' button")
                        saveNvram()
                        Toast.makeText(this@MainActivity, "NVRAM saved", Toast.LENGTH_SHORT).show()
                    }
                    presentationView.findViewById<Button>(R.id.btnPalette).setOnClickListener {
                        Log.i(TAG, "User tapped 'PALETTE' button")
                        cyclePalette()
                    }
                    presentationView.findViewById<Button>(R.id.btnReset).setOnClickListener {
                        Log.i(TAG, "User tapped 'RESET' button")
                        showResetDialog()
                    }
                }

                override fun onBackPressed() {
                    /* Forward to Cybiko ESC key instead of dismissing presentation */
                    CybikoNative.nativeSendKey(CybikoNative.KEY_ESC, true)
                }

                override fun dispatchKeyEvent(event: KeyEvent): Boolean {
                    if (event.keyCode == KeyEvent.KEYCODE_BACK) {
                        if (event.action == KeyEvent.ACTION_DOWN && event.repeatCount == 0) {
                            CybikoNative.nativeSendKey(CybikoNative.KEY_ESC, true)
                        }
                        return true
                    }
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

            keyboardPresentation?.setOnDismissListener {
                Log.w(TAG, "Presentation dismissed; re-showing on secondary display")
                if (!isFinishing && !isDestroyed) {
                    setupDualDisplayIfAvailable()
                }
            }

            try {
                keyboardPresentation?.show()
                Log.i(TAG, "Keyboard Presentation successfully shown on secondary display")
            } catch (e: Exception) {
                Log.e(TAG, "Failed to show Presentation on secondary display", e)
                bottomContainer.visibility = View.VISIBLE
            }
        } else {
            Log.i(TAG, "Single display layout active (split top/bottom)")
            bottomContainer.visibility = View.VISIBLE
        }
    }

    private fun installAppFromUri(uri: Uri) {
        try {
            val fileName = getFileName(uri) ?: "game.app"
            if (!fileName.endsWith(".app", ignoreCase = true) && !fileName.endsWith(".fls", ignoreCase = true)) {
                Toast.makeText(this, "Please select a Cybiko application (.app)", Toast.LENGTH_LONG).show()
                return
            }

            contentResolver.openInputStream(uri)?.use { input ->
                val appBytes = input.readBytes()
                val success = CybikoNative.nativeInjectApp(fileName, appBytes)
                if (success) {
                    saveNvram()
                    Toast.makeText(this, "Installed '$fileName' to Cybiko!", Toast.LENGTH_SHORT).show()
                    Log.i(TAG, "Successfully installed app '$fileName' (${appBytes.size} bytes)")
                } else {
                    Toast.makeText(this, "Failed to install '$fileName' (CFS filesystem full?)", Toast.LENGTH_LONG).show()
                    Log.w(TAG, "Failed to install app '$fileName'")
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed reading app from URI $uri", e)
            Toast.makeText(this, "Error installing app: ${e.message}", Toast.LENGTH_LONG).show()
        }
    }

    private fun getFileName(uri: Uri): String? {
        var name: String? = null
        if (uri.scheme == "content") {
            try {
                contentResolver.query(uri, null, null, null, null)?.use { cursor ->
                    if (cursor.moveToFirst()) {
                        val idx = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                        if (idx >= 0) name = cursor.getString(idx)
                    }
                }
            } catch (e: Exception) {
                Log.w(TAG, "Could not resolve display name from content resolver", e)
            }
        }
        if (name == null) {
            name = uri.lastPathSegment?.let { File(it).name }
        }
        return name
    }

    private fun cyclePalette() {
        currentPalette = (currentPalette + 1) % 3
        CybikoNative.nativeSetPalette(currentPalette)
        val name = when (currentPalette) {
            CybikoNative.PALETTE_CYBIKO_GREEN -> "Authentic LCD Green"
            CybikoNative.PALETTE_GRAYSCALE -> "Pocket High-Contrast B&W"
            CybikoNative.PALETTE_AMBER -> "Cyber Amber"
            else -> "Default"
        }
        Log.i(TAG, "Palette switched to $name ($currentPalette)")
        Toast.makeText(this, "Palette: $name", Toast.LENGTH_SHORT).show()
    }

    private fun showResetDialog() {
        AlertDialog.Builder(this)
            .setTitle("Cybiko Reset Options")
            .setMessage("Select reboot action:")
            .setPositiveButton("Soft Reboot") { _, _ ->
                CybikoNative.nativeReset()
                Toast.makeText(this, "Cybiko rebooted", Toast.LENGTH_SHORT).show()
            }
            .setNeutralButton("Factory Reset (Wipe NVRAM)") { _, _ ->
                performFactoryReset()
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun performFactoryReset() {
        stopEmulationLoop()
        val nvramFile = File(filesDir, "cybiko.nvram")
        if (nvramFile.exists()) {
            nvramFile.delete()
        }
        loadFirmwareAndNvram()
        startEmulationLoop()
        Toast.makeText(this, "NVRAM wiped. Initial setup restarted.", Toast.LENGTH_LONG).show()
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

            /* Auto-inject bundled apps from assets/apps/ if nvram didn't exist */
            if (nvramBytes == null) {
                try {
                    val appFiles = assets.list("apps") ?: emptyArray()
                    for (appName in appFiles) {
                        if (appName.endsWith(".app", ignoreCase = true)) {
                            assets.open("apps/$appName").use { input ->
                                val appData = input.readBytes()
                                val injected = CybikoNative.nativeInjectApp(appName, appData)
                                Log.i(TAG, "Auto-injected app '$appName' (success=$injected)")
                            }
                        }
                    }
                } catch (e: Exception) {
                    Log.w(TAG, "Failed to load bundled apps from assets", e)
                }
            }
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
            var lastAutoSaveTime = System.currentTimeMillis()

            while (isRunning.get()) {
                val frameStart = System.nanoTime()

                /* Run 1 core frame (~307,200 CPU cycles) */
                CybikoNative.nativeRunFrame()

                /* Blit frame to screen */
                displayView.renderFrame(
                    CybikoNative.nativeGetFps(),
                    CybikoNative.nativeGetCoreMs()
                )

                /* Periodic NVRAM auto-save every 30 seconds */
                val now = System.currentTimeMillis()
                if (now - lastAutoSaveTime >= 30_000L) {
                    saveNvram()
                    lastAutoSaveTime = now
                }

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
        if (event.keyCode == KeyEvent.KEYCODE_BACK) {
            if (event.action == KeyEvent.ACTION_DOWN && event.repeatCount == 0) {
                CybikoNative.nativeSendKey(CybikoNative.KEY_ESC, true)
            }
            return true
        }
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
