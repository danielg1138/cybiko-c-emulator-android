# Cybiko Xtreme Emulator

A Cybiko Xtreme (XT) handheld computer emulator written in C, ported from a Java implementation. Boots and runs CyOS with full CPU, display, audio, keyboard, and filesystem emulation.

## Features

- **H8S/2323 CPU** - Full instruction set emulation with interrupt handling and DMA
- **HD66421 LCD** - 160x100 pixel, 4-shade grayscale display
- **Audio** - 1-bit speaker via Timer16 PWM output compare
- **Keyboard** - 10-column matrix with Fn-layer for numbers
- **CFS** - Cybiko File System with app injection
- **NVRAM** - Persistent save state across sessions
- **App loading** - Inject `.app` files into the filesystem at boot

## Building

Requires CMake 3.16+ and SDL2.

```bash
# Install SDL2 (Ubuntu/Debian)
sudo apt-get install libsdl2-dev

# Build
cmake -B build
cmake --build build
```

## Running

```bash
./build/cybiko-emu <boot.bin> <flash.bin> [options]
```

| Option | Description |
|--------|-------------|
| `--nvram <file>` | Load/save NVRAM state (created on exit if absent) |
| `--app <file>` | Inject an `.app` file into the filesystem (repeatable) |
| `--trace` | Enable CPU instruction tracing to stderr |

### Loading Apps

```bash
./build/cybiko-emu roms/boot.bin roms/flash.bin --nvram save.nvram --app game.app
```

If the NVRAM file exists, it is loaded as the base filesystem. Otherwise a fresh CFS is formatted. Apps are injected by filename before boot. NVRAM is written back on exit.

## Keyboard

| PC Key | Cybiko Key |
|--------|------------|
| A-Z | A-Z |
| 1-9, 0 | Fn + Q/W/E/R/T/Y/U/I/O/P |
| Enter | Enter |
| Space | Space |
| Backspace / Delete | Del |
| Tab | Tab |
| Escape | Esc |
| Arrow keys | D-pad |
| Home | Select |
| Insert | As |
| End | Help |
| Period | . |
| Semicolon | ; |
| Comma | , |
| L/R Ctrl | Fn |
| L/R Shift | Shift |
| F1-F7 | Soft keys |

## Architecture

```
src/
  core/           Platform-independent emulator
    h8s_cpu.c       H8S/2323 CPU instruction decoder
    address_bus.c   Memory map and I/O routing
    memory.c        Generic memory regions
    hd66421.c       HD66421 LCD controller
    timer8.c        8-bit timer (TMR)
    timer16.c       16-bit timer (TPU) with PWM output
    speaker.c       1-bit audio from timer PWM
    keyboard.c      10-column keyboard matrix
    cfs.c           Cybiko File System
    rtc.c           PCF8593 real-time clock (I2C)
    emulator.c      Orchestrator / frame loop
  hal/
    hal.h           Hardware abstraction layer interface
  linux/
    main.c          SDL2 frontend (display, keyboard, audio)
tests/
    test_*.c        Unit tests (acutest framework)
```

The `cybiko_core` library has no external dependencies and can be used with any frontend. The HAL interface provides callbacks for display, audio, keyboard polling, and serial output.

## Testing

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Runs unit tests for all core modules: memory, CFS, keyboard, timer8, timer16, speaker, HD66421 LCD, and H8S CPU.

## Links

- [Cybiko Archive (archive.org)](https://archive.org/details/cybiko)
- [MAME Cybiko XT source](https://github.com/mamedev/mame/blob/master/src/mame/cybiko/cybiko.cpp)
- [Web emulator](https://cybiko.kyul.is/)
