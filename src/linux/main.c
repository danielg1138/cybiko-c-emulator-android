/*
 * Cybiko Emulator - SDL2 Linux Client
 *
 * Boots the Cybiko XT emulator with SDL2 display, keyboard, and audio.
 *
 * Usage: ./cybiko-emu <boot.bin> <flash.bin> [--nvram file.nvram] [--trace]
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "core/emulator.h"

/* ---------------------------------------------------------------------------
 * SDL context - holds handles needed by HAL callbacks
 * --------------------------------------------------------------------------- */

typedef struct {
    SDL_Renderer    *renderer;
    SDL_Texture     *texture;
    SDL_AudioDeviceID audio_dev;
    uint16_t         key_matrix[CYBIKO_KEYBOARD_COLUMNS];
} sdl_ctx_t;

/* ---------------------------------------------------------------------------
 * HAL callbacks
 * --------------------------------------------------------------------------- */

/* render_frame: grayscale pixels (0-255) -> ARGB8888 texture -> present */
static void hal_render_frame(void *ctx, const uint8_t *pixels, int w, int h)
{
    sdl_ctx_t *sdl = ctx;
    uint32_t *tex_pixels;
    int pitch;

    SDL_LockTexture(sdl->texture, NULL, (void **)&tex_pixels, &pitch);
    for (int i = 0; i < w * h; i++) {
        uint8_t g = pixels[i];
        tex_pixels[i] = 0xFF000000u | ((uint32_t)g << 16) | ((uint32_t)g << 8) | g;
    }
    SDL_UnlockTexture(sdl->texture);
    SDL_RenderCopy(sdl->renderer, sdl->texture, NULL, NULL);
    SDL_RenderPresent(sdl->renderer);
}

/* keyboard_poll: copy key matrix state into emulator buffer */
static void hal_keyboard_poll(void *ctx, uint16_t *matrix, int num_cols)
{
    sdl_ctx_t *sdl = ctx;
    int cols = num_cols < CYBIKO_KEYBOARD_COLUMNS ? num_cols : CYBIKO_KEYBOARD_COLUMNS;
    memcpy(matrix, sdl->key_matrix, (size_t)cols * sizeof(uint16_t));
}

/* audio_output: queue raw samples for playback */
static void hal_audio_output(void *ctx, const uint8_t *samples, int count)
{
    sdl_ctx_t *sdl = ctx;
    SDL_QueueAudio(sdl->audio_dev, samples, (Uint32)count);
}

/* serial_output: print debug byte to stderr */
static void hal_serial_output(void *ctx, uint8_t byte)
{
    (void)ctx;
    fputc(byte, stderr);
}

/* ---------------------------------------------------------------------------
 * Keyboard mapping (placeholder - full mapping comes later)
 * --------------------------------------------------------------------------- */

static void update_key_matrix(sdl_ctx_t *sdl, SDL_KeyboardEvent *key)
{
    bool pressed = (key->type == SDL_KEYDOWN);
    int col = -1;
    uint16_t mask = 0;

    switch (key->keysym.scancode) {
        case SDL_SCANCODE_UP:     col =  9; mask = 0x0002; break;
        case SDL_SCANCODE_DOWN:   col = 10; mask = 0x0002; break;
        case SDL_SCANCODE_LEFT:   col = 11; mask = 0x0002; break;
        case SDL_SCANCODE_RIGHT:  col = 12; mask = 0x0002; break;
        case SDL_SCANCODE_RETURN: col =  8; mask = 0x0002; break;
        case SDL_SCANCODE_ESCAPE: col = 13; mask = 0x0002; break;
        default: break;
    }

    if (col >= 0 && col < CYBIKO_KEYBOARD_COLUMNS) {
        if (pressed)
            sdl->key_matrix[col] |= mask;
        else
            sdl->key_matrix[col] &= ~mask;
    }
}

/* ---------------------------------------------------------------------------
 * File loading helper
 * --------------------------------------------------------------------------- */

static uint8_t *load_file(const char *path, size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Error: cannot open '%s': %s\n", path, strerror(errno));
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    if (len < 0) {
        fprintf(stderr, "Error: cannot determine size of '%s'\n", path);
        fclose(f);
        return NULL;
    }
    fseek(f, 0, SEEK_SET);

    uint8_t *buf = malloc((size_t)len);
    if (!buf) {
        fprintf(stderr, "Error: out of memory loading '%s'\n", path);
        fclose(f);
        return NULL;
    }

    size_t nread = fread(buf, 1, (size_t)len, f);
    fclose(f);

    if (nread != (size_t)len) {
        fprintf(stderr, "Error: short read on '%s' (expected %ld, got %zu)\n",
                path, len, nread);
        free(buf);
        return NULL;
    }

    *out_size = (size_t)len;
    return buf;
}

/* ---------------------------------------------------------------------------
 * Main
 * --------------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    /* ---- Parse command-line arguments ---- */
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <boot.bin> <flash.bin> [--nvram file] [--trace]\n",
                argv[0]);
        return 1;
    }

    const char *boot_path  = argv[1];
    const char *flash_path = argv[2];
    const char *nvram_path = NULL;
    bool trace = false;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--nvram") == 0 && i + 1 < argc) {
            nvram_path = argv[++i];
        } else if (strcmp(argv[i], "--trace") == 0) {
            trace = true;
        } else {
            fprintf(stderr, "Warning: unknown option '%s'\n", argv[i]);
        }
    }

    (void)trace; /* Used by build system via CYBIKO_TRACE define */

    /* ---- Load ROM files ---- */
    size_t boot_size = 0, flash_size = 0, nvram_size = 0;

    uint8_t *boot_data = load_file(boot_path, &boot_size);
    if (!boot_data) return 1;

    uint8_t *flash_data = load_file(flash_path, &flash_size);
    if (!flash_data) { free(boot_data); return 1; }

    uint8_t *nvram_data = NULL;
    if (nvram_path) {
        nvram_data = load_file(nvram_path, &nvram_size);
        /* NVRAM file may not exist yet - that's okay on first run */
        if (!nvram_data) {
            fprintf(stderr, "Note: NVRAM file '%s' not found, starting fresh\n",
                    nvram_path);
        }
    }

    /* ---- Initialize SDL2 ---- */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "Error: SDL_Init failed: %s\n", SDL_GetError());
        free(boot_data);
        free(flash_data);
        free(nvram_data);
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Cybiko Emulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        CYBIKO_LCD_WIDTH * 3, CYBIKO_LCD_HEIGHT * 3,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        fprintf(stderr, "Error: SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        free(boot_data);
        free(flash_data);
        free(nvram_data);
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED
    );
    if (!renderer) {
        fprintf(stderr, "Error: SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        free(boot_data);
        free(flash_data);
        free(nvram_data);
        return 1;
    }

    SDL_Texture *texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        CYBIKO_LCD_WIDTH, CYBIKO_LCD_HEIGHT
    );
    if (!texture) {
        fprintf(stderr, "Error: SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        free(boot_data);
        free(flash_data);
        free(nvram_data);
        return 1;
    }

    /* ---- Open audio device ---- */
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq     = 44100;
    want.format   = AUDIO_U8;
    want.channels = 1;
    want.samples  = 2048;

    SDL_AudioDeviceID audio_dev = SDL_OpenAudioDevice(
        NULL, 0, &want, &have, 0
    );
    if (audio_dev == 0) {
        fprintf(stderr, "Warning: SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        /* Continue without audio */
    }

    /* ---- Set up SDL context and HAL ---- */
    sdl_ctx_t sdl_ctx;
    memset(&sdl_ctx, 0, sizeof(sdl_ctx));
    sdl_ctx.renderer  = renderer;
    sdl_ctx.texture   = texture;
    sdl_ctx.audio_dev = audio_dev;

    cybiko_hal_t hal;
    hal.render_frame  = hal_render_frame;
    hal.audio_output  = hal_audio_output;
    hal.keyboard_poll = hal_keyboard_poll;
    hal.serial_output = hal_serial_output;
    hal.ctx           = &sdl_ctx;

    /* ---- Create emulator and load ROMs ---- */
    cybiko_emu_t *emu = cybiko_create(&hal);
    if (!emu) {
        fprintf(stderr, "Error: cybiko_create() failed\n");
        if (audio_dev) SDL_CloseAudioDevice(audio_dev);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        free(boot_data);
        free(flash_data);
        free(nvram_data);
        return 1;
    }

    if (!cybiko_load_boot_rom(emu, boot_data, boot_size)) {
        fprintf(stderr, "Error: failed to load boot ROM '%s'\n", boot_path);
        cybiko_destroy(emu);
        if (audio_dev) SDL_CloseAudioDevice(audio_dev);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        free(boot_data);
        free(flash_data);
        free(nvram_data);
        return 1;
    }

    if (!cybiko_load_flash_rom(emu, flash_data, flash_size)) {
        fprintf(stderr, "Error: failed to load flash ROM '%s'\n", flash_path);
        cybiko_destroy(emu);
        if (audio_dev) SDL_CloseAudioDevice(audio_dev);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        free(boot_data);
        free(flash_data);
        free(nvram_data);
        return 1;
    }

    if (nvram_data) {
        if (!cybiko_load_nvram(emu, nvram_data, nvram_size)) {
            fprintf(stderr, "Warning: failed to load NVRAM data\n");
        }
    }

    /* ROM data has been copied into the emulator - free the buffers */
    free(boot_data);
    free(flash_data);
    free(nvram_data);

    /* ---- Start emulation ---- */
    cybiko_reset(emu);

    /* Unpause audio device to start playback */
    if (audio_dev) {
        SDL_PauseAudioDevice(audio_dev, 0);
    }

    /* ---- Main loop ---- */
    bool running = true;
    while (running) {
        uint32_t frame_start = SDL_GetTicks();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                case SDL_KEYUP:
                    update_key_matrix(&sdl_ctx, &event.key);
                    break;
                default:
                    break;
            }
        }

        cybiko_run_frame(emu);

        /* Frame-rate limiting (~60 FPS) */
        uint32_t elapsed = SDL_GetTicks() - frame_start;
        if (elapsed < 16) {
            SDL_Delay(16 - elapsed);
        }
    }

    /* ---- Save NVRAM if path was provided ---- */
    if (nvram_path) {
        size_t nvram_len = 0;
        const uint8_t *nvram = cybiko_get_nvram(emu, &nvram_len);
        if (nvram && nvram_len > 0) {
            FILE *f = fopen(nvram_path, "wb");
            if (f) {
                fwrite(nvram, 1, nvram_len, f);
                fclose(f);
                fprintf(stderr, "NVRAM saved to '%s' (%zu bytes)\n",
                        nvram_path, nvram_len);
            } else {
                fprintf(stderr, "Warning: could not save NVRAM to '%s'\n",
                        nvram_path);
            }
        }
    }

    /* ---- Cleanup ---- */
    cybiko_destroy(emu);
    if (audio_dev) SDL_CloseAudioDevice(audio_dev);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
