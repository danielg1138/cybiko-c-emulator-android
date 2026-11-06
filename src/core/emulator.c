/*
 * Cybiko emulator orchestrator.
 *
 * Ties together CPU, address bus, LCD, timers, RTC, speaker, and keyboard
 * into a single runnable emulator driven by a platform HAL.
 */
#include "emulator.h"
#include "h8s_cpu.h"
#include "address_bus.h"
#include "hd66421.h"
#include "timer8.h"
#include "timer16.h"
#include "rtc.h"
#include "speaker.h"
#include "keyboard.h"
#include "cfs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct cybiko_emu {
    cybiko_hal_t hal;

    h8s_cpu_t     cpu;
    address_bus_t bus;
    hd66421_t     lcd;
    timer8_t      timer8[2];
    timer16_t     timer16[6];
    rtc_t         rtc;
    speaker_t     speaker;
    keyboard_t    keyboard;

    bool     running;
    int      cpi;
    uint64_t total_steps;
    uint32_t frame_count;
};

/* Timer16 channel 1 TIOCB1 output compare → speaker (Port 1 bit 3) */
static void timer16_ch1_tiocb_cb(void *ctx, int level) {
    cybiko_emu_t *emu = ctx;
    speaker_set_level(&emu->speaker, level);
}

/* ---------- lifecycle ---------- */

cybiko_emu_t *cybiko_create(const cybiko_hal_t *hal) {
    cybiko_emu_t *emu = calloc(1, sizeof(cybiko_emu_t));
    if (!emu) return NULL;

    /* Copy the HAL callbacks */
    emu->hal = *hal;

    /* Initialize the bus itself (must come before memory_init - zeroes struct) */
    bus_init(&emu->bus);

    /* Initialize memory regions in the bus */
    memory_init(&emu->bus.boot_rom,     0x8000,   false);  /* 32 KB, read-only  */
    memory_init(&emu->bus.external_ram, 0x200000, true);   /* 2 MB, writable    */
    memory_init(&emu->bus.flash_rom,    0x80000,  false);  /* 512 KB, read-only */
    memory_init(&emu->bus.on_chip_ram,  0x2400,   true);   /* ~10 KB, writable  */

    /* Wire peripherals into bus */
    emu->bus.lcd       = &emu->lcd;
    emu->bus.timer8[0] = &emu->timer8[0];
    emu->bus.timer8[1] = &emu->timer8[1];
    for (int i = 0; i < 6; i++) {
        emu->bus.timer16[i] = &emu->timer16[i];
    }
    emu->bus.rtc      = &emu->rtc;
    emu->bus.speaker  = &emu->speaker;
    emu->bus.keyboard = &emu->keyboard;

    /* Initialize peripherals */
    hd66421_init(&emu->lcd);

    timer8_init(&emu->timer8[0], 0, &emu->cpu);
    timer8_init(&emu->timer8[1], 1, &emu->cpu);

    /* Timer16 channels: (channel, tgrCount, baseVector, cpu) */
    timer16_init(&emu->timer16[0], 0, 4, 32, &emu->cpu);
    timer16_init(&emu->timer16[1], 1, 2, 40, &emu->cpu);
    timer16_init(&emu->timer16[2], 2, 2, 44, &emu->cpu);
    timer16_init(&emu->timer16[3], 3, 4, 48, &emu->cpu);
    timer16_init(&emu->timer16[4], 4, 2, 56, &emu->cpu);
    timer16_init(&emu->timer16[5], 5, 2, 60, &emu->cpu);

    /* Wire Timer16 ch1 TIOCB1 output compare to speaker */
    emu->timer16[1].output_b_cb = timer16_ch1_tiocb_cb;
    emu->timer16[1].output_b_ctx = emu;

    rtc_init(&emu->rtc);
    speaker_init(&emu->speaker, CYBIKO_CLOCK_HZ);
    keyboard_init(&emu->keyboard, CYBIKO_KEYBOARD_SCAN_COLS);

    /* Initialize CPU (needs bus already wired) */
    h8s_cpu_init(&emu->cpu, &emu->bus);

    /* Wire CPU back into bus (for interrupt delivery, etc.) */
    emu->bus.cpu = &emu->cpu;

    emu->running = true;
    emu->cpi = 4; /* Default to Authentic 1.0x hardware speed */

    return emu;
}

void cybiko_destroy(cybiko_emu_t *emu) {
    if (!emu) return;
    memory_free(&emu->bus.boot_rom);
    memory_free(&emu->bus.external_ram);
    memory_free(&emu->bus.flash_rom);
    memory_free(&emu->bus.on_chip_ram);
    free(emu);
}

/* ---------- ROM / NVRAM loading ---------- */

bool cybiko_load_boot_rom(cybiko_emu_t *emu, const uint8_t *data, size_t len) {
    memory_load(&emu->bus.boot_rom, data, len, 0);
    return true;
}

bool cybiko_load_flash_rom(cybiko_emu_t *emu, const uint8_t *data, size_t len) {
    memory_load(&emu->bus.flash_rom, data, len, 0);
    return true;
}

bool cybiko_load_nvram(cybiko_emu_t *emu, const uint8_t *data, size_t len) {
    memory_load(&emu->bus.external_ram, data, len, 0);
    return true;
}

/* ---------- reset ---------- */

void cybiko_reset(cybiko_emu_t *emu) {
    h8s_cpu_reset(&emu->cpu);
}

/* ---------- main frame loop ---------- */

void cybiko_run_frame(cybiko_emu_t *emu) {
    /* Poll keyboard state from the HAL */
    if (emu->hal.keyboard_poll) {
        emu->hal.keyboard_poll(emu->hal.ctx,
                               emu->keyboard.columns,
                               CYBIKO_KEYBOARD_COLUMNS);
    }

    /* Cache which timers are running to avoid per-cycle checks */
    bool t8_0 = timer8_is_running(&emu->timer8[0]);
    bool t8_1 = timer8_is_running(&emu->timer8[1]);
    bool t16[6];
    for (int i = 0; i < 6; i++) {
        t16[i] = timer16_is_running(&emu->timer16[i]);
    }

    /* Begin speaker frame (record start level, reset transitions) */
    speaker_begin_frame(&emu->speaker);

    int cpi = emu->cpi > 0 ? emu->cpi : 4;

    /* Execute one frame worth of CPU cycles */
    int cycle = 0;
    while (cycle < CYBIKO_CYCLES_PER_FRAME) {
        /* Track cycle position for speaker transition timestamps */
        emu->speaker.frame_cycle = cycle;

        /* Tick timers for 'cpi' clock cycles */
        for (int k = 0; k < cpi; k++) {
            if (t8_0) timer8_tick(&emu->timer8[0]);
            if (t8_1) timer8_tick(&emu->timer8[1]);
            for (int i = 0; i < 6; i++) {
                if (t16[i]) timer16_tick(&emu->timer16[i]);
            }
        }

        /* CPU step */
        h8s_cpu_step(&emu->cpu);

        /* DMA completion */
        bus_tick_dma_completion(&emu->bus);

        emu->total_steps++;
        cycle += cpi;
    }

    /* Render the frame via HAL */
    if (emu->hal.render_frame) {
        const uint8_t *pixels = hd66421_render(&emu->lcd);
        emu->hal.render_frame(emu->hal.ctx, pixels,
                              CYBIKO_LCD_WIDTH, CYBIKO_LCD_HEIGHT);
    }

    /* Generate audio samples for this frame */
    if (emu->hal.audio_output) {
        uint8_t audio_buf[8192];
        int samples = speaker_generate_samples(&emu->speaker,
                                               CYBIKO_CYCLES_PER_FRAME,
                                               audio_buf, sizeof(audio_buf));
        if (samples > 0) {
            emu->hal.audio_output(emu->hal.ctx, audio_buf, samples);
        }
    }

    /* Tick RTC once per frame */
    bus_tick_rtc(&emu->bus);

    /* Drain serial output buffers to the HAL */
    if (emu->hal.serial_output) {
        for (int ch = 0; ch < 3; ch++) {
            for (int i = 0; i < emu->bus.sci_output_pos[ch]; i++) {
                emu->hal.serial_output(emu->hal.ctx,
                                       (uint8_t)emu->bus.sci_output[ch][i]);
            }
            emu->bus.sci_output_pos[ch] = 0;
        }
    }

    emu->frame_count++;
}

/* ---------- queries ---------- */

bool cybiko_is_running(const cybiko_emu_t *emu) {
    return emu->running && !emu->cpu.halted;
}

const uint8_t *cybiko_get_nvram(const cybiko_emu_t *emu, size_t *len) {
    if (len) *len = emu->bus.external_ram.size;
    return memory_raw((memory_t *)&emu->bus.external_ram);
}

void cybiko_set_speed_mode(cybiko_emu_t *emu, cybiko_speed_mode_t mode) {
    if (!emu) return;
    switch (mode) {
    case CYBIKO_SPEED_AUTHENTIC:
        emu->cpi = 4; /* 1.0x Authentic H8S speed (~76.8k inst/frame) */
        break;
    case CYBIKO_SPEED_FAST:
        emu->cpi = 2; /* 2.0x Fast Forward (~153.6k inst/frame) */
        break;
    case CYBIKO_SPEED_TURBO:
        emu->cpi = 1; /* 4.0x Turbo (~307.2k inst/frame) */
        break;
    case CYBIKO_SPEED_CLASSIC:
        emu->cpi = 6; /* 0.67x Cybiko Classic 11 MHz (~51.2k inst/frame) */
        break;
    default:
        emu->cpi = 4;
        break;
    }
}

cybiko_speed_mode_t cybiko_get_speed_mode(const cybiko_emu_t *emu) {
    if (!emu) return CYBIKO_SPEED_AUTHENTIC;
    if (emu->cpi == 4) return CYBIKO_SPEED_AUTHENTIC;
    if (emu->cpi == 2) return CYBIKO_SPEED_FAST;
    if (emu->cpi == 1) return CYBIKO_SPEED_TURBO;
    if (emu->cpi == 6) return CYBIKO_SPEED_CLASSIC;
    return CYBIKO_SPEED_AUTHENTIC;
}

void cybiko_set_cpi(cybiko_emu_t *emu, int cpi) {
    if (emu && cpi >= 1 && cpi <= 16) {
        emu->cpi = cpi;
    }
}

int cybiko_get_cpi(const cybiko_emu_t *emu) {
    return emu ? emu->cpi : 4;
}
