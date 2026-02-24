# Cybiko C Emulator

Cybiko XT emulator ported from Java to C, targeting Linux (SDL2) and ESP32-S3 (T-Deck Pro).

## Architecture

- **Core** (`src/core/`): Platform-independent emulator (H8S/2323 CPU, address bus, LCD, timers, RTC, speaker, keyboard)
- **HAL** (`src/hal/`): Hardware Abstraction Layer interface
- **Linux** (`src/linux/`): SDL2 frontend (display, keyboard, audio)

## Reference Implementations

- **Web emulator** (`../website clones/cybiko_xtreme/`): Newer, confirmed working. **Use as primary reference** for keyboard mappings and behavior.
- **Java emulator** (`../cybiko-java-emulator/`): Original port source. Has known keyboard column bug (P on column 13).
- **MAME** (`../mame/`): Older, incomplete XT support. Keyboard column definitions for XT are wrong (P on column 13 instead of column 9).

## Keyboard Column Fix (Critical)

The Cybiko XT keyboard matrix has 10 scan columns (0-9). CyOS scans these via DMA using 10-bit word offsets. MAME and the Java emulator incorrectly place P, period, and Help on column 13, which causes column aliasing: since DMA word offsets are only 10 bits, bits 10-14 are always 0, meaning columns 10-14 leak into every DMA scan result.

**Correct mapping (from web emulator):**
- Column 9: Help (0x0001), period (0x0002), semicolon (0x0008), P (0x0010)
- On/Off: NOT in keyboard matrix; read via Port A GPIO (bit 7)

**Wrong mapping (MAME/Java):**
- Column 13: Help (0x0001), period (0x0002), P (0x0010) -- CAUSES ALIASING
- Column 14: On/Off (0x8000) -- CAUSES ALIASING

The `keyboard_read()` function scans only columns 0-9 (`CYBIKO_KEYBOARD_SCAN_COLS = 10`). The array is sized to 15 for HAL communication but only 10 columns are scanned.

## DMA Notes

- CyOS programs DMACR=0x8007: 16-bit mode, both source and destination FIXED
- DMACR FAE mode bit layout (from MAME h8_dma.cpp):
  - High byte: bit 7=MODE_16, bit 6=SRC_DEC, bit 5=SRC_INC_EN (0=fixed)
  - Low byte: bit 6=DST_DEC, bit 5=DST_INC_EN (0=fixed)

## Build

```bash
cd build && cmake .. && cmake --build .
./cybiko-emu <boot.bin> <flash.bin> [--nvram file.nvram]
```
