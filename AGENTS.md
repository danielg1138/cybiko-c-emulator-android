# Cybiko Android Emulator (Ayn Thor) — Agent Operating Rules

These rules govern all development, porting, debugging, optimization, and testing in this workspace.

## Rule 1: Strictly Separate "Hypothesis" from "Confirmed Fact"
- **Never** label a theory as a "Root Cause" or "Confirmed" in chat, code comments, or artifacts until a counter, log trace, debugger breakpoint, or A/B test has proven it.
- Read-only code inspection only ever produces a **Hypothesis**. Frame it explicitly as: *"Hypothesis: X may be happening. Measurement to prove/disprove: Y."*
- Trust the user's visual, audio, and gameplay observations on the Ayn Thor over theoretical expectations. If an emulator fix was expected to solve an issue and the user still observes it, either the hypothesis or the deployment is wrong.

## Rule 2: Instrument & Measure Before Optimizing ("Telemetry First")
- Do not write an optimization for a suspected bottleneck until a logcat trace or timer proves that code path accounts for a meaningful share of frame time.
- Emulation runs at a target 60 FPS (~16.6ms per frame). Measure and report actual core execution time (`cybiko_run_frame`), render blit time, and audio buffer latency before touching core emulation loops.
- Frame pacing, audio underruns/overruns, and input latency must be verified with explicit metrics (`[cybiko-telemetry]`).

## Rule 3: Strict Decoupling of Core and Android Frontend
- **Never** introduce Android-, POSIX-, or platform-specific dependencies into `src/core/` or `src/hal/`. `cybiko_core` must remain 100% pure, platform-independent C11.
- All Android JNI bindings, input mapping, OpenGL ES/Vulkan rendering, and audio backends (Oboe/AAudio/AudioTrack) must reside exclusively in the frontend layer (`src/android/`).
- Visual enhancements (scaling filters, CRT/LCD dot-matrix shaders, bezel overlays) must be kept strictly separate from core emulation and memory bus logic.

## Rule 4: Mandatory Pre-Launch Deployment & Environment Checklist
Before deploying and launching on the Ayn Thor or an Android test environment, verify every item on this checklist:
1. **Target ABI & Architecture Check:**
   - Target `arm64-v8a` (Snapdragon 865 / 8 Gen 2). Verify NDK build outputs exist and match ABI.
2. **ROM & Asset Verification:**
   - Confirm Cybiko boot ROM (`boot.bin` / `cyrom150.bin`) and CyOS firmware (`flash.bin` / `cyos_v1508.bin`) are accessible in app-accessible storage or scoped storage.
3. **Audio Configuration:**
   - Ensure audio conversion (8-bit mono 48 kHz → 16-bit PCM / AAudio stream) is correctly initialized and not blocking the emulation thread.
4. **Input Debounce / Hold Synchronization:**
   - Confirm the 3-frame hold minimum and Fn-key timing logic from `src/linux/main.c` is preserved in the Android touch/gamepad input driver so CyOS does not drop keypresses.

## Rule 5: Automated Smoke Check Before Asking the User to Playtest
- Inspect Android `logcat` within the first 5–15 seconds of launch to verify:
  1. `cybiko_create()`, `cybiko_load_boot_rom()`, and `cybiko_load_flash_rom()` returned `true` with no file I/O or null-pointer errors.
  2. Frame callback is firing at ~60 FPS without unbounded audio queue growth or starvation.
  3. No fatal JNI exceptions or unhandled native signals (`SIGSEGV`, `SIGILL`).
- If an exception or stall occurs in logs, diagnose and resolve it before requesting hardware playtesting.

## Rule 6: Concise, Evidence-Backed Documentation & Hard ABI Constraints
- Keep artifacts short, structured, and table-driven: **Hypothesis → Measurement → Result → Verdict**.
- Do not alter CPU instruction decode tables or keyboard matrix column maps without cross-referencing known Cybiko hardware documentation (MAME / Web emulator matrix quirks).

## Rule 7: Mandatory Run Log Documentation
- After completing analysis of a test run or significant implementation milestone, document the findings in `RUN_LOG.md`:
  1. Update the **Run Summary Table** with: Run #, target/build, test scenario, primary metric (frame time / audio latency), and status/verdict.
  2. Add details in the strict format: **Hypothesis → Measurement → Result → Verdict**, quoting exact logcat output.
