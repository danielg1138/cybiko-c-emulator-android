# Cybiko Android Emulator — Run Log

## Run Summary Table

| Run # | Target / Commit | Test Scenario | Primary Metric | Bottleneck / Notes | Status / Verdict |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **001** | `origin/main` | Host C11 Unit Tests (`clang -std=c11`) | 8/8 test suites pass | Clean host build | **PASS** (Baseline validated) |
| **002** | NDK r30 Clang | `arm64-v8a` Cross-Compilation of `cybiko_core` | 11/11 translation units compile | Zero compiler warnings | **PASS** (Core ABI confirmed) |
| **003** | Host C11 runner | `c4pc` Boot ROM test (`emu_rom.bin`) | PC resets to `0x001010` | Hits `0x200100` (Classic vs XT memory map) | **CONFIRMED** (Classic vs XT diff verified) |
| **004** | NDK r30 CMake | Android JNI Bridge & HAL build (`libcybiko.so`) | Clean link with AAudio/log/jnigraphics | 14/14 translation units compiled | **PASS** (`arm64-v8a` binary ready) |
| **005** | Gradle / AGP 8.5.2 | Full Android APK Build (`assembleDebug`) | Packaging `app-debug.apk` (7.3 MB) | Verified `lib/arm64-v8a/libcybiko.so` inside APK | **PASS** (Milestone 1 APK Ready) |
| **006** | Host Clang Unit Test | Android Bridge Native Logic (`test_android_bridge.c`) | 4/4 verification suites pass | Debounce, Fn-timing, PCM conversion, column bounds | **PASS** (Android bridge validated) |
| **007** | Cybiko XT MAME ROMs | Automated Boot Smoke Test (`test_xt_boot.c`) | 312.72 FPS (3.20 ms/frame) | Bootloader -> DRAM 2048K -> LZSS decomp -> CyOS 0x4a3bf8 | **PASS** (5.2x real-time speed) |
| **008** | Gradle / AGP | APK Assembly with Bundled Assets | `app-debug.apk` (7.8 MB) | Auto-extracts `boot.bin` & `flash.bin` on first launch | **PASS** (Zero-setup APK ready) |
| **009** | Ayn Thor (`3767dd3c`) | Hardware Boot, Dual-Screen, CFS & -O3 Optimization | 3.80 ms - 4.20 ms (59.5-60.4 FPS) | Top screen 10x integer scale, bottom screen Presentation keyboard, physical Button A maps to Enter, CFS 512 KB formatted | **PASS** (Hardware 60 FPS validated) |
| **010** | Ayn Thor (`3767dd3c`) | Clamshell Toolbar, Back-to-ESC Mapping, SAF Loader | 4.16 ms - 4.47 ms (58.4-60.5 FPS) | Back key maps to Cybiko ESC (no dialog dismiss), bottom toolbar (+LOAD .APP, SAVE, PALETTE, RESET), 30s auto-save verified | **PASS** (App loading & toolbar operational) |
| **011** | Ayn Thor (`3767dd3c`) | Dual-Screen Lifecycle Sync & In-App App Browser | 4.14 ms - 4.30 ms (58.9-60.0 FPS) | Synchronized onStart/onStop presentation dismiss on Home (melonDS/Azahar pattern); in-app file browser lists .app files directly | **PASS** (Screen sync & file browser verified) |

---

## Detailed Run Entries

### Run 004: Native Android JNI & HAL Cross-Compilation
- **Hypothesis:** `src/android/cybiko_jni.c`, `cybiko_audio.c`, and `cybiko_input.c` integrate cleanly with `src/core/` and compile to `libcybiko.so` under NDK r30 Clang targeting `arm64-v8a` (Android 26+).
- **Measurement:** Executed CMake with `android.toolchain.cmake`, `-DANDROID_ABI=arm64-v8a`, `-DANDROID_PLATFORM=android-26`.
- **Result:** `libcybiko.so` built successfully (ELF 64-bit LSB shared object, ARM aarch64). 0 errors, 0 warnings.
- **Verdict:** PASS. Native JNI frontend verified.

### Run 005: Full Android Application Package Assembly
- **Hypothesis:** Full Gradle Android pipeline builds, compiles Kotlin dual-screen views, compiles native `libcybiko.so`, and packages a valid Android debug APK for `arm64-v8a`.
- **Measurement:** Executed `gradle assembleDebug` with NDK r30 and Android SDK API 34.
- **Result:** Build succeeded (`BUILD SUCCESSFUL in 3s`). Output APK `android/app/build/outputs/apk/debug/app-debug.apk` inspected: contains `classes.dex`, `AndroidManifest.xml`, and `lib/arm64-v8a/libcybiko.so`.
- **Verdict:** PASS. Milestone 1 scaffolding complete.

### Run 006: Android Native Bridge Logic Verification
- **Hypothesis:** `cybiko_input` accurately implements the 3-frame hold minimum, Fn-key timing sequence for numbers, and scancode isolation, preventing CyOS dropped inputs and column aliasing.
- **Measurement:** Executed `tests/test_android_bridge.c` covering hold timers, Fn-delay states, PCM conversion mathematics, and scan column limits.
- **Result:**
  ```text
  === Running Android Bridge Native Tests ===
  [PASS] test_key_hold_minimum
  [PASS] test_number_key_fn_timing
  [PASS] test_audio_pcm_conversion
  [PASS] test_matrix_column_bounds
  === ALL ANDROID BRIDGE TESTS PASSED (4/4) ===
  ```
- **Verdict:** PASS. Android input engine and audio conversions validated.

### Run 007: Cybiko XT Official Firmware Automated Boot Smoke Test
- **Hypothesis:** `cyrom150.bin` (32 KB boot ROM) and `cyos_v1508.bin` (512 KB CyOS) boot successfully through the H8S CPU orchestrator and initialize CyOS desktop without runtime exceptions.
- **Measurement:** Executed `tests/test_xt_boot.c` for 180 continuous emulation frames (~921,600 CPU cycles), capturing serial stdout, LCD pixel writes, and audio buffer output.
- **Result:**
  ```text
  === Starting Cybiko XT Firmware Automated Boot Smoke Test ===
  [PASS] cybiko_create() succeeded
  [PASS] Loaded boot ROM (32768 bytes)
  [PASS] Loaded flash ROM (524288 bytes)
  [PASS] CPU reset executed. Running 180 frames (3 seconds @ 60 FPS)...
  Starting CyOS boot loader v1.5.0.
  Detecting DRAM interface settings... 10-bit shift.
  Detecting DRAM size... 2048K.
  Scanning flash: plain image found.
  Preparing to load CyOS.
  Got header: magic 1234ABCD : plain image.
  Starting 0x6001c8....
  Got header: magic = 1C0FFAB, compressed_size = 117922, plain_size = 216348: LZSS compressed image.
  decompressing....
  Got header: magic = 1234ABCD, compressed_size = 4865016, plain_size = 6297700: plain image.
  Starting 0x4a3bf8....

  --- Smoke Test Telemetry Results ---
  Total Frames Rendered: 180
  Total Emulation Time:  0.576 s
  Average Core Speed:    312.72 FPS (3.20 ms/frame | Target budget: 16.6 ms)
  Speedup factor:        5.2x real-time
  Audio Callbacks:       180 (144000 total samples generated)
  Active LCD Pixels:     13243 / 16000
  === AUTOMATED SMOKE TEST VERDICT: SUCCESS ===
  ```
- **Verdict:** PASS. CyOS 1.5.08 boots cleanly. Performance is 5.2x faster than real-time (3.20 ms vs 16.6 ms frame budget).

### Run 008: Out-of-the-Box Self-Contained APK Packaging
- **Hypothesis:** Packaging verified XT firmware into APK assets enables immediate out-of-the-box operation on the Ayn Thor without requiring manual ADB file transfer.
- **Measurement:** Added asset extraction logic to `MainActivity.kt`, bundled `boot.bin` and `flash.bin` into `android/app/src/main/assets/roms/`, and assembled debug APK.
- **Result:** Built `app-debug.apk` (7.8 MB). `unzip -l` confirms `assets/roms/boot.bin`, `assets/roms/flash.bin`, and `lib/arm64-v8a/libcybiko.so` are present inside APK.
- **Verdict:** PASS. Zero-setup installation confirmed.

### Run 009: Ayn Thor Physical Hardware Deployment, Dual-Screen & -O3 Optimization
- **Hypothesis:** Compiling with `-O3 -fomit-frame-pointer` will bring core frame times under the 16.6 ms budget on the Ayn Thor Snapdragon chipset, while CFS auto-formatting and Presentation window key dispatch will allow full dual-screen interaction with CyOS.
- **Measurement:** Deployed to Ayn Thor (`3767dd3c`), tested dual-screen rendering (Top 1080p display ID 0, Bottom Presentation display ID 4), recorded logcat telemetry for frame timing and input matrix synchronization.
- **Result:**
  - Frame time dropped from **28.35 ms** (`-O0`) to **3.80 ms - 4.20 ms** (`-O3`), locking solid **59.5–60.4 FPS** (~24% frame budget utilization).
  - Presentation window successfully initialized on Display 4 with sticky immersive mode and haptic QWERTY keyboard.
  - Presentation window key dispatch overrode physical gamepad button events (`ThorGamepadMapper`), routing Button A (`KEYCODE_BUTTON_A`) to Cybiko Enter (`CYBIKO_KEY_ENTER`).
  - Added `pthread_mutex_t input_mutex` in `cybiko_jni.c` to prevent race conditions between UI touch events and the 60 FPS emulation thread.
  - Increased `CYBIKO_MIN_HOLD_FRAMES` to 6 frames (~100 ms hold) so CyOS keyboard scanning reliably detects fast touch and button taps.
  - Added auto-format `cfs_format()` (524,544 bytes) when uninitialized NVRAM is detected, allowing CyOS to mount its filesystem and advance past the setup wizard.
  - User successfully advanced CyOS through the First-Boot Setup Wizard (Welcome -> Contrast -> Time/Date -> Nickname screen).
- **Verdict:** PASS. Verified end-to-end on Ayn Thor physical hardware.

### Run 010: Clamshell Toolbar, Hardware Back Mapping & SAF App Loader
- **Hypothesis:** Intercepting `KEYCODE_BACK` at both `Presentation` and `MainActivity` level and routing to `CYBIKO_KEY_ESC` will prevent hardware back presses from dismissing the secondary display dialog while allowing natural menu back navigation in CyOS; adding a bottom screen toolbar with Storage Access Framework (SAF) will enable one-tap live app injection into CFS.
- **Measurement:** Deployed update to Ayn Thor (`3767dd3c`), sent `KEYCODE_BACK` events while viewing dialogs, verified Presentation retention via `screencap -d 4630946482288158084`, checked logcat for 30s NVRAM auto-saving, and loaded CyOS Main Desktop.
- **Result:**
  - Hardware Back press successfully mapped to Cybiko Escape (`CYBIKO_KEY_ESC`), closing OS dialogs and returning to Main Desktop without closing keyboard or activity.
  - Bottom display layout updated with compact action toolbar (`+ LOAD .APP`, `SAVE`, `PALETTE`, `RESET`) above full QWERTY touch matrix.
  - Periodic auto-save verified: `NVRAM successfully persisted (2097152 bytes)` written every 30 seconds.
  - Pushed curated test library (`pinball_pro.app`, `Homerun_X.app`, `tetris.app`, `Snake.app`, `BTower_X.app`) to `/sdcard/Download/Cybiko_Games/` for testing with SAF file picker.
  - Core frame time: **4.16 ms - 4.47 ms** (**58.4 - 60.5 FPS**).
- **Verdict:** PASS. Clamshell toolbar and game loader verified operational.

### Run 011: Synchronized Dual-Screen Lifecycle & In-App App Browser
- **Hypothesis:** Tying `keyboardPresentation` lifecycle explicitly to `onStart()` (attach) and `onStop()` (dismiss) will mirror melonDS/Azahar behavior, ensuring both clamshell screens dismiss and reappear together when entering Home or multitasking; implementing an in-app file browser dialog will bypass Android DocumentsUI's MIME-type filtering issue for unknown `.app` extensions.
- **Measurement:** Sent `KEYCODE_HOME` on Ayn Thor (`3767dd3c`), confirmed both top and bottom screens exited to launcher via `screencap`, reopened Cybiko from launcher and verified both screens re-attached cleanly. Tapped `+ LOAD .APP`, verified in-app file browser displayed `.app` files with sizes, selected `pinball_pro.app`, and confirmed CFS injection in logcat.
- **Result:**
  - `KEYCODE_HOME` immediately dismissed `keyboardPresentation` in `onStop()`; screencap confirmed secondary display returned to Android launcher wallpaper.
  - Re-launching app from launcher triggered `onStart()` which cleanly re-attached `keyboardPresentation` on Display 4.
  - In-app app browser dialog opened, listing files in `/sdcard/Download/Cybiko_Games` (`BTower_X.app`, `Homerun_X.app`, `pinball_pro.app`, `Snake.app`, `tetris.app`).
  - Selecting `pinball_pro.app` injected 19,121 bytes directly into CFS and persisted 2,097,152 bytes NVRAM.
- **Verdict:** PASS. Dual-screen lifecycle synchronization and in-app file picker validated on hardware.





### Run 001: Core Unit Test Baseline
- **Hypothesis:** All 8 unit tests in `tests/` pass unmodified under standard C11.
- **Measurement:** Compiled all `tests/test_*.c` against `src/core/*.c` using Clang.
- **Result:**
  ```text
  === tests/test_cfs.c === SUCCESS
  === tests/test_h8s_cpu.c === SUCCESS
  === tests/test_hd66421.c === SUCCESS
  === tests/test_keyboard.c === SUCCESS
  === tests/test_memory.c === SUCCESS
  === tests/test_speaker.c === SUCCESS
  === tests/test_timer16.c === SUCCESS
  === tests/test_timer8.c === SUCCESS
  ```
- **Verdict:** PASS. Base emulator core is functionally intact.

### Run 002: Android NDK `arm64-v8a` Cross-Compilation Smoke Check
- **Hypothesis:** `src/core/` contains no platform-specific dependencies and can compile cleanly for `aarch64-linux-android30` with NDK Clang.
- **Measurement:** Invoked `aarch64-linux-android30-clang -std=c11 -Isrc -c src/core/*.c`.
- **Result:** All 11 object files created as `ELF 64-bit LSB relocatable, ARM aarch64`. 0 warnings, 0 errors.
- **Verdict:** PASS. Target architecture verified.

### Run 003: Cybiko Classic `c4pc` ROM Execution Smoke Test
- **Hypothesis:** Running `emu_rom.bin` in the current XT emulator core will execute instructions but deviate due to memory map divergence.
- **Measurement:** Loaded `emu_rom.bin` and `emu_cyos.bin` via a headless harness and executed 10 frames.
- **Result:** CPU booted from reset vector and executed code at `PC=0x001010`, but encountered unmapped memory writes at `0x200100` (expected for Classic vs XT).
- **Verdict:** CONFIRMED. Identifies exact memory map differences for Milestone 2 combo emulator support.
