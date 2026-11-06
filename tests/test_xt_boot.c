#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "core/emulator.h"
#include "android/cybiko_audio.h"
#include "android/cybiko_input.h"

typedef struct {
    int frame_count;
    int audio_calls;
    int total_audio_samples;
    uint32_t non_white_pixels;
    uint8_t last_frame[CYBIKO_LCD_WIDTH * CYBIKO_LCD_HEIGHT];
} test_ctx_t;

static void test_render_frame(void *ctx, const uint8_t *pixels, int w, int h)
{
    test_ctx_t *t = (test_ctx_t *)ctx;
    t->frame_count++;
    memcpy(t->last_frame, pixels, (size_t)(w * h));

    uint32_t non_white = 0;
    for (int i = 0; i < w * h; i++) {
        if (pixels[i] < 200) {
            non_white++;
        }
    }
    t->non_white_pixels = non_white;
}

static void test_audio_output(void *ctx, const uint8_t *samples, int count)
{
    test_ctx_t *t = (test_ctx_t *)ctx;
    t->audio_calls++;
    t->total_audio_samples += count;
}

static void test_keyboard_poll(void *ctx, uint16_t *matrix, int num_cols)
{
    (void)ctx;
    memset(matrix, 0, (size_t)num_cols * sizeof(uint16_t));
}

static void test_serial_output(void *ctx, uint8_t byte)
{
    (void)ctx;
    fputc(byte, stdout);
}

int main(void) {
    printf("=== Starting Cybiko XT Firmware Automated Boot Smoke Test ===\n");

    test_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    cybiko_hal_t hal;
    hal.render_frame  = test_render_frame;
    hal.audio_output  = test_audio_output;
    hal.keyboard_poll = test_keyboard_poll;
    hal.serial_output = test_serial_output;
    hal.ctx           = &ctx;

    /* 1. cybiko_create() */
    cybiko_emu_t *emu = cybiko_create(&hal);
    if (!emu) {
        printf("[FAIL] cybiko_create() returned NULL\n");
        return 1;
    }
    printf("[PASS] cybiko_create() succeeded\n");

    /* 2. Load boot.bin */
    FILE *fboot = fopen("roms/boot.bin", "rb");
    if (!fboot) {
        printf("[FAIL] Cannot open roms/boot.bin\n");
        cybiko_destroy(emu);
        return 1;
    }
    fseek(fboot, 0, SEEK_END);
    long boot_len = ftell(fboot);
    fseek(fboot, 0, SEEK_SET);
    uint8_t *boot_buf = malloc((size_t)boot_len);
    fread(boot_buf, 1, (size_t)boot_len, fboot);
    fclose(fboot);

    if (!cybiko_load_boot_rom(emu, boot_buf, (size_t)boot_len)) {
        printf("[FAIL] cybiko_load_boot_rom() returned false\n");
        free(boot_buf);
        cybiko_destroy(emu);
        return 1;
    }
    printf("[PASS] Loaded boot ROM (%ld bytes)\n", boot_len);
    free(boot_buf);

    /* 3. Load flash.bin */
    FILE *fflash = fopen("roms/flash.bin", "rb");
    if (!fflash) {
        printf("[FAIL] Cannot open roms/flash.bin\n");
        cybiko_destroy(emu);
        return 1;
    }
    fseek(fflash, 0, SEEK_END);
    long flash_len = ftell(fflash);
    fseek(fflash, 0, SEEK_SET);
    uint8_t *flash_buf = malloc((size_t)flash_len);
    fread(flash_buf, 1, (size_t)flash_len, fflash);
    fclose(fflash);

    if (!cybiko_load_flash_rom(emu, flash_buf, (size_t)flash_len)) {
        printf("[FAIL] cybiko_load_flash_rom() returned false\n");
        free(flash_buf);
        cybiko_destroy(emu);
        return 1;
    }
    printf("[PASS] Loaded flash ROM (%ld bytes)\n", flash_len);
    free(flash_buf);

    /* 4. Reset CPU and run frames */
    cybiko_reset(emu);
    printf("[PASS] CPU reset executed. Running 180 frames (3 seconds @ 60 FPS)...\n");

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    const int FRAMES_TO_RUN = 180;
    for (int f = 0; f < FRAMES_TO_RUN; f++) {
        cybiko_run_frame(emu);
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    uint64_t total_ns = (uint64_t)(t1.tv_sec - t0.tv_sec) * 1000000000ULL +
                        (uint64_t)(t1.tv_nsec - t0.tv_nsec);
    double total_sec = (double)total_ns / 1e9;
    double fps = (double)FRAMES_TO_RUN / total_sec;
    double ms_per_frame = (total_sec * 1000.0) / (double)FRAMES_TO_RUN;

    printf("\n--- Smoke Test Telemetry Results ---\n");
    printf("Total Frames Rendered: %d\n", ctx.frame_count);
    printf("Total Emulation Time:  %.3f s\n", total_sec);
    printf("Average Core Speed:    %.2f FPS (%.2f ms/frame | Target budget: 16.6 ms)\n", fps, ms_per_frame);
    printf("Speedup factor:        %.1fx real-time\n", fps / 60.0);
    printf("Audio Callbacks:       %d (%d total samples generated)\n", ctx.audio_calls, ctx.total_audio_samples);
    printf("Active LCD Pixels:     %u / %d\n", ctx.non_white_pixels, CYBIKO_LCD_WIDTH * CYBIKO_LCD_HEIGHT);

    cybiko_destroy(emu);

    if (ctx.frame_count == FRAMES_TO_RUN && fps > 60.0) {
        printf("=== AUTOMATED SMOKE TEST VERDICT: SUCCESS ===\n");
        return 0;
    } else {
        printf("=== AUTOMATED SMOKE TEST VERDICT: FAILED ===\n");
        return 1;
    }
}
