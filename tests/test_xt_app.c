#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "core/emulator.h"
#include "core/cfs.h"

typedef struct {
    int frame_count;
    uint32_t active_pixels;
    uint16_t key_matrix[CYBIKO_KEYBOARD_COLUMNS];
} test_ctx_t;

static void hal_render_frame(void *ctx, const uint8_t *pixels, int w, int h)
{
    test_ctx_t *t = (test_ctx_t *)ctx;
    t->frame_count++;
    t->active_pixels = 0;
    for (int i = 0; i < w * h; i++) {
        if (pixels[i] < 240) t->active_pixels++;
    }
    if (t->frame_count == 400 || t->frame_count == 450 || t->frame_count == 520 || t->frame_count == 850 || t->frame_count == 1300) {
        char fname[64];
        snprintf(fname, sizeof(fname), "/tmp/frame_%04d.ppm", t->frame_count);
        FILE *fp = fopen(fname, "wb");
        if (fp) {
            fprintf(fp, "P6\n%d %d\n255\n", w, h);
            for (int i = 0; i < w * h; i++) {
                uint8_t p = pixels[i];
                fputc(p, fp); fputc(p, fp); fputc(p, fp);
            }
            fclose(fp);
        }
    }
}

static void hal_keyboard_poll(void *ctx, uint16_t *matrix, int num_cols)
{
    test_ctx_t *t = (test_ctx_t *)ctx;
    int cols = num_cols < CYBIKO_KEYBOARD_COLUMNS ? num_cols : CYBIKO_KEYBOARD_COLUMNS;
    memcpy(matrix, t->key_matrix, (size_t)cols * sizeof(uint16_t));
}

static void hal_audio_output(void *ctx, const uint8_t *samples, int count)
{
    (void)ctx;
    (void)samples;
    (void)count;
}

static void hal_serial_output(void *ctx, uint8_t byte)
{
    (void)ctx;
    fputc(byte, stderr);
}

int main(void)
{
    test_ctx_t ctx = {0};
    cybiko_hal_t hal = {
        .render_frame = hal_render_frame,
        .keyboard_poll = hal_keyboard_poll,
        .audio_output = hal_audio_output,
        .serial_output = hal_serial_output,
        .ctx = &ctx
    };

    cybiko_emu_t *emu = cybiko_create(&hal);
    if (!emu) return 1;

    /* 1. Boot ROM */
    FILE *fb = fopen("roms/boot.bin", "rb");
    if (!fb) return 2;
    fseek(fb, 0, SEEK_END);
    size_t blen = (size_t)ftell(fb);
    fseek(fb, 0, SEEK_SET);
    uint8_t *bbuf = malloc(blen);
    fread(bbuf, 1, blen, fb);
    fclose(fb);
    cybiko_load_boot_rom(emu, bbuf, blen);
    free(bbuf);

    /* 2. Flash ROM */
    FILE *ff = fopen("roms/flash.bin", "rb");
    if (!ff) return 3;
    fseek(ff, 0, SEEK_END);
    size_t flen = (size_t)ftell(ff);
    fseek(ff, 0, SEEK_SET);
    uint8_t *fbuf = malloc(flen);
    fread(fbuf, 1, flen, ff);
    fclose(ff);
    cybiko_load_flash_rom(emu, fbuf, flen);
    free(fbuf);

    /* 3. CFS with BTower_X.app */
    cfs_image_t *cfs = malloc(sizeof(cfs_image_t));
    cfs_format(cfs);

    FILE *fa = fopen("cybiko/games/btower/BTower_X.app", "rb");
    if (fa) {
        fseek(fa, 0, SEEK_END);
        size_t alen = (size_t)ftell(fa);
        fseek(fa, 0, SEEK_SET);
        uint8_t *abuf = malloc(alen);
        fread(abuf, 1, alen, fa);
        fclose(fa);

        bool added = cfs_add_file(cfs, "BTower_X.app", abuf, alen);
        printf("cfs_add_file BTower_X.app: %s\n", added ? "SUCCESS" : "FAILED");
        free(abuf);
    }
    cybiko_load_nvram(emu, cfs->data, CFS_IMAGE_SIZE);
    free(cfs);

    cybiko_reset(emu);

    printf("Running to frame 400...\n");
    for (int i = 0; i < 400; i++) {
        cybiko_run_frame(emu);
    }

    printf("Pressing ENTER at frame 400 (welcome)...\n");
    ctx.key_matrix[4] |= 0x0008;
    for (int i = 400; i < 410; i++) {
        cybiko_run_frame(emu);
    }
    printf("Releasing ENTER at frame 410...\n");
    ctx.key_matrix[4] &= ~0x0008;
    for (int i = 410; i < 460; i++) {
        cybiko_run_frame(emu);
    }

    printf("Pressing ENTER at frame 460 (contrast)...\n");
    ctx.key_matrix[4] |= 0x0008;
    for (int i = 460; i < 470; i++) {
        cybiko_run_frame(emu);
    }
    printf("Releasing ENTER at frame 470...\n");
    ctx.key_matrix[4] &= ~0x0008;
    for (int i = 470; i < 550; i++) {
        cybiko_run_frame(emu);
    }

    printf("Pressing ENTER at frame 550 (time/date)...\n");
    ctx.key_matrix[4] |= 0x0008;
    for (int i = 550; i < 560; i++) {
        cybiko_run_frame(emu);
    }
    printf("Releasing ENTER at frame 560...\n");
    ctx.key_matrix[4] &= ~0x0008;
    for (int i = 560; i < 1000; i++) {
        cybiko_run_frame(emu);
    }

    printf("Typing DAN on nickname screen...\n");
    /* Delete key: Column 5, 0x0100 */
    for (int k = 0; k < 8; k++) {
        ctx.key_matrix[5] |= 0x0100;
        for (int i = 0; i < 5; i++) cybiko_run_frame(emu);
        ctx.key_matrix[5] &= ~0x0100;
        for (int i = 0; i < 5; i++) cybiko_run_frame(emu);
    }

    /* Press 'D': Column 2, 0x0100 */
    ctx.key_matrix[2] |= 0x0100;
    for (int i = 0; i < 5; i++) cybiko_run_frame(emu);
    ctx.key_matrix[2] &= ~0x0100;
    for (int i = 0; i < 5; i++) cybiko_run_frame(emu);

    /* Press 'A': Column 3, 0x0004 */
    ctx.key_matrix[3] |= 0x0004;
    for (int i = 0; i < 5; i++) cybiko_run_frame(emu);
    ctx.key_matrix[3] &= ~0x0004;
    for (int i = 0; i < 5; i++) cybiko_run_frame(emu);

    /* Press 'N': Column 1, 0x0008 */
    ctx.key_matrix[1] |= 0x0008;
    for (int i = 0; i < 5; i++) cybiko_run_frame(emu);
    ctx.key_matrix[1] &= ~0x0008;
    for (int i = 0; i < 20; i++) cybiko_run_frame(emu);

    /* Press ENTER: Column 4, 0x0008 */
    ctx.key_matrix[4] |= 0x0008;
    for (int i = 0; i < 10; i++) cybiko_run_frame(emu);
    ctx.key_matrix[4] &= ~0x0008;
    for (int i = 0; i < 300; i++) cybiko_run_frame(emu);

    printf("\nAfter 400 frames: active_pixels = %u\n", ctx.active_pixels);
    cybiko_destroy(emu);
    return 0;
}
