#include <jni.h>
#include <android/log.h>
#include <android/bitmap.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "core/emulator.h"
#include "core/cfs.h"
#include "cybiko_audio.h"
#include "cybiko_input.h"

#define TAG "CybikoJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)

/* ---------------------------------------------------------------------------
 * Authentic LCD Color Palettes (ARGB8888)
 * --------------------------------------------------------------------------- */
static const uint32_t PALETTES[3][4] = {
    /* 0: Authentic Greenish Cybiko LCD */
    { 0xFF1C2818u, 0xFF445834u, 0xFF748C5Cu, 0xFF9CB87Eu },
    /* 1: Monochromatic Neutral Grayscale */
    { 0xFF101010u, 0xFF585858u, 0xFFA0A0A0u, 0xFFECECECu },
    /* 2: Amber Backlit LCD */
    { 0xFF181004u, 0xFF583808u, 0xFFA06810u, 0xFFFFB828u }
};

typedef struct {
    cybiko_emu_t         *emu;
    cybiko_audio_t       *audio;
    cybiko_input_state_t  input;
    pthread_mutex_t       render_mutex;
    pthread_mutex_t       input_mutex;

    /* Double-buffered 160x100 RGBA frame buffer */
    uint32_t              frame_rgba[CYBIKO_LCD_WIDTH * CYBIKO_LCD_HEIGHT];
    bool                  frame_ready;
    int                   palette_mode;

    /* Telemetry counters (Rule 2) */
    uint64_t              frame_count;
    uint64_t              total_core_time_ns;
    struct timespec       last_telemetry_time;
    float                 current_fps;
    float                 avg_core_ms;
} android_ctx_t;

static android_ctx_t g_ctx;
static bool g_initialized = false;

/* ---------------------------------------------------------------------------
 * HAL Callbacks
 * --------------------------------------------------------------------------- */

static void hal_render_frame(void *ctx, const uint8_t *pixels, int width, int height)
{
    android_ctx_t *actx = (android_ctx_t *)ctx;
    const uint32_t *palette = PALETTES[actx->palette_mode % 3];

    pthread_mutex_lock(&actx->render_mutex);
    int total = width * height;
    for (int i = 0; i < total; i++) {
        uint8_t p = pixels[i];
        /* Cybiko pixels are 0..255; map into 4 palette bins */
        int shade = (p < 64) ? 0 : (p < 128) ? 1 : (p < 192) ? 2 : 3;
        actx->frame_rgba[i] = palette[shade];
    }
    actx->frame_ready = true;
    pthread_mutex_unlock(&actx->render_mutex);
}

static void hal_audio_output(void *ctx, const uint8_t *samples, int count)
{
    android_ctx_t *actx = (android_ctx_t *)ctx;
    if (actx->audio) {
        cybiko_audio_push_u8(actx->audio, samples, count);
    }
}

static void hal_keyboard_poll(void *ctx, uint16_t *matrix, int num_cols)
{
    android_ctx_t *actx = (android_ctx_t *)ctx;
    pthread_mutex_lock(&actx->input_mutex);
    cybiko_input_copy_matrix(&actx->input, matrix, num_cols);
    pthread_mutex_unlock(&actx->input_mutex);
}

static void hal_serial_output(void *ctx, uint8_t byte)
{
    (void)ctx;
    static char line_buf[256];
    static int line_idx = 0;

    if (byte == '\n' || byte == '\r' || line_idx >= 254) {
        if (line_idx > 0) {
            line_buf[line_idx] = '\0';
            __android_log_print(ANDROID_LOG_DEBUG, "CybikoSerial", "%s", line_buf);
            line_idx = 0;
        }
    } else if (byte >= 32 && byte <= 126) {
        line_buf[line_idx++] = (char)byte;
    }
}

/* ---------------------------------------------------------------------------
 * JNI Lifecycle & Operations
 * --------------------------------------------------------------------------- */

JNIEXPORT jboolean JNICALL
Java_com_cybiko_emulator_CybikoNative_nativeInit(JNIEnv *env, jclass clazz, jint palette_mode)
{
    (void)env;
    (void)clazz;
    if (g_initialized) return JNI_TRUE;

    memset(&g_ctx, 0, sizeof(g_ctx));
    pthread_mutex_init(&g_ctx.render_mutex, NULL);
    pthread_mutex_init(&g_ctx.input_mutex, NULL);
    g_ctx.palette_mode = (palette_mode >= 0 && palette_mode <= 2) ? palette_mode : 0;

    cybiko_input_init(&g_ctx.input);
    g_ctx.audio = cybiko_audio_create();
    if (g_ctx.audio) {
        cybiko_audio_start(g_ctx.audio);
    }

    cybiko_hal_t hal;
    hal.render_frame  = hal_render_frame;
    hal.audio_output  = hal_audio_output;
    hal.keyboard_poll = hal_keyboard_poll;
    hal.serial_output = hal_serial_output;
    hal.ctx           = &g_ctx;

    g_ctx.emu = cybiko_create(&hal);
    if (!g_ctx.emu) {
        LOGE("Failed to create Cybiko emulator core instance");
        if (g_ctx.audio) cybiko_audio_destroy(g_ctx.audio);
        pthread_mutex_destroy(&g_ctx.render_mutex);
        return JNI_FALSE;
    }

    clock_gettime(CLOCK_MONOTONIC, &g_ctx.last_telemetry_time);
    g_initialized = true;
    LOGI("CybikoNative initialized successfully (palette=%d)", g_ctx.palette_mode);
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_cybiko_emulator_CybikoNative_nativeDestroy(JNIEnv *env, jclass clazz)
{
    (void)env;
    (void)clazz;
    if (!g_initialized) return;

    if (g_ctx.emu) {
        cybiko_destroy(g_ctx.emu);
        g_ctx.emu = NULL;
    }
    if (g_ctx.audio) {
        cybiko_audio_destroy(g_ctx.audio);
        g_ctx.audio = NULL;
    }
    pthread_mutex_destroy(&g_ctx.render_mutex);
    pthread_mutex_destroy(&g_ctx.input_mutex);
    g_initialized = false;
    LOGI("CybikoNative destroyed");
}

JNIEXPORT jboolean JNICALL
Java_com_cybiko_emulator_CybikoNative_nativeLoadRoms(
    JNIEnv *env, jclass clazz,
    jbyteArray boot_array,
    jbyteArray flash_array,
    jbyteArray nvram_array)
{
    (void)clazz;
    if (!g_initialized || !g_ctx.emu) return JNI_FALSE;

    /* 1. Boot ROM */
    if (boot_array) {
        jsize boot_len = (*env)->GetArrayLength(env, boot_array);
        jbyte *boot_ptr = (*env)->GetByteArrayElements(env, boot_array, NULL);
        if (!cybiko_load_boot_rom(g_ctx.emu, (const uint8_t *)boot_ptr, (size_t)boot_len)) {
            LOGE("Failed to load boot ROM (%d bytes)", (int)boot_len);
            (*env)->ReleaseByteArrayElements(env, boot_array, boot_ptr, JNI_ABORT);
            return JNI_FALSE;
        }
        (*env)->ReleaseByteArrayElements(env, boot_array, boot_ptr, JNI_ABORT);
        LOGI("Loaded boot ROM (%d bytes)", (int)boot_len);
    }

    /* 2. Flash ROM */
    if (flash_array) {
        jsize flash_len = (*env)->GetArrayLength(env, flash_array);
        jbyte *flash_ptr = (*env)->GetByteArrayElements(env, flash_array, NULL);
        if (!cybiko_load_flash_rom(g_ctx.emu, (const uint8_t *)flash_ptr, (size_t)flash_len)) {
            LOGE("Failed to load flash ROM (%d bytes)", (int)flash_len);
            (*env)->ReleaseByteArrayElements(env, flash_array, flash_ptr, JNI_ABORT);
            return JNI_FALSE;
        }
        (*env)->ReleaseByteArrayElements(env, flash_array, flash_ptr, JNI_ABORT);
        LOGI("Loaded flash ROM (%d bytes)", (int)flash_len);
    }

    /* 3. NVRAM / CFS Filesystem */
    if (nvram_array) {
        jsize nvram_len = (*env)->GetArrayLength(env, nvram_array);
        if (nvram_len >= CFS_IMAGE_SIZE) {
            jbyte *nvram_ptr = (*env)->GetByteArrayElements(env, nvram_array, NULL);
            cybiko_load_nvram(g_ctx.emu, (const uint8_t *)nvram_ptr, (size_t)nvram_len);
            (*env)->ReleaseByteArrayElements(env, nvram_array, nvram_ptr, JNI_ABORT);
            LOGI("Loaded NVRAM (%d bytes)", (int)nvram_len);
        } else {
            cfs_image_t *cfs = malloc(sizeof(cfs_image_t));
            if (cfs) {
                cfs_format(cfs);
                cybiko_load_nvram(g_ctx.emu, cfs->data, CFS_IMAGE_SIZE);
                free(cfs);
                LOGI("NVRAM too small (%d bytes); formatted fresh CFS (%d bytes)", (int)nvram_len, CFS_IMAGE_SIZE);
            }
        }
    } else {
        /* Format a fresh CFS filesystem so CyOS has a valid file system to mount */
        cfs_image_t *cfs = malloc(sizeof(cfs_image_t));
        if (cfs) {
            cfs_format(cfs);
            cybiko_load_nvram(g_ctx.emu, cfs->data, CFS_IMAGE_SIZE);
            free(cfs);
            LOGI("No NVRAM provided; formatted fresh CFS filesystem (%d bytes)", CFS_IMAGE_SIZE);
        }
    }

    cybiko_reset(g_ctx.emu);
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_cybiko_emulator_CybikoNative_nativeInjectApp(
    JNIEnv *env, jclass clazz,
    jstring name_str,
    jbyteArray data_array)
{
    (void)clazz;
    if (!g_initialized || !g_ctx.emu || !name_str || !data_array) return JNI_FALSE;

    const char *name = (*env)->GetStringUTFChars(env, name_str, NULL);
    jsize data_len = (*env)->GetArrayLength(env, data_array);
    jbyte *data_ptr = (*env)->GetByteArrayElements(env, data_array, NULL);

    cfs_image_t *cfs = malloc(sizeof(cfs_image_t));
    if (!cfs) {
        (*env)->ReleaseStringUTFChars(env, name_str, name);
        (*env)->ReleaseByteArrayElements(env, data_array, data_ptr, JNI_ABORT);
        return JNI_FALSE;
    }

    size_t cur_len = 0;
    const uint8_t *cur_nvram = cybiko_get_nvram(g_ctx.emu, &cur_len);
    if (cur_nvram && cur_len >= CFS_IMAGE_SIZE) {
        memcpy(cfs->data, cur_nvram, CFS_IMAGE_SIZE);
    } else {
        cfs_format(cfs);
    }

    bool added = cfs_add_file(cfs, name, (const uint8_t *)data_ptr, (size_t)data_len);
    if (added) {
        cybiko_load_nvram(g_ctx.emu, cfs->data, CFS_IMAGE_SIZE);
        LOGI("Injected app '%s' (%d bytes) into CFS", name, (int)data_len);
    } else {
        LOGE("Failed to inject app '%s' (CFS capacity exceeded?)", name);
    }

    free(cfs);
    (*env)->ReleaseStringUTFChars(env, name_str, name);
    (*env)->ReleaseByteArrayElements(env, data_array, data_ptr, JNI_ABORT);
    return added ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_cybiko_emulator_CybikoNative_nativeReset(JNIEnv *env, jclass clazz)
{
    (void)env;
    (void)clazz;
    if (g_initialized && g_ctx.emu) {
        cybiko_reset(g_ctx.emu);
        if (g_ctx.audio) cybiko_audio_clear(g_ctx.audio);
    }
}

JNIEXPORT void JNICALL
Java_com_cybiko_emulator_CybikoNative_nativeRunFrame(JNIEnv *env, jclass clazz)
{
    (void)env;
    (void)clazz;
    if (!g_initialized || !g_ctx.emu) return;

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    /* Update input hold timers before frame cycle */
    pthread_mutex_lock(&g_ctx.input_mutex);
    cybiko_input_tick_frame(&g_ctx.input);
    pthread_mutex_unlock(&g_ctx.input_mutex);

    cybiko_run_frame(g_ctx.emu);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    uint64_t elapsed_ns = (uint64_t)(t1.tv_sec - t0.tv_sec) * 1000000000ULL +
                          (uint64_t)(t1.tv_nsec - t0.tv_nsec);

    /* Telemetry tracking (Rule 2) */
    g_ctx.frame_count++;
    g_ctx.total_core_time_ns += elapsed_ns;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    uint64_t sec_diff = (uint64_t)(now.tv_sec - g_ctx.last_telemetry_time.tv_sec);
    if (sec_diff >= 1) {
        uint64_t ns_diff = sec_diff * 1000000000ULL +
                           (uint64_t)(now.tv_nsec - g_ctx.last_telemetry_time.tv_nsec);
        g_ctx.current_fps = (float)g_ctx.frame_count * 1000000000.0f / (float)ns_diff;
        g_ctx.avg_core_ms = ((float)g_ctx.total_core_time_ns / (float)g_ctx.frame_count) / 1000000.0f;

        LOGD("[cybiko-telemetry] fps=%.1f core=%.2fms/frame (budget=16.6ms)",
             g_ctx.current_fps, g_ctx.avg_core_ms);

        g_ctx.frame_count = 0;
        g_ctx.total_core_time_ns = 0;
        g_ctx.last_telemetry_time = now;
    }
}

JNIEXPORT void JNICALL
Java_com_cybiko_emulator_CybikoNative_nativeSendKey(
    JNIEnv *env, jclass clazz, jint key_id, jboolean pressed)
{
    (void)env;
    (void)clazz;
    if (!g_initialized) return;
    LOGD("nativeSendKey(key=%d, pressed=%d)", (int)key_id, (int)pressed);
    pthread_mutex_lock(&g_ctx.input_mutex);
    cybiko_input_set_key(&g_ctx.input, (int)key_id, (bool)pressed);
    pthread_mutex_unlock(&g_ctx.input_mutex);
}

JNIEXPORT void JNICALL
Java_com_cybiko_emulator_CybikoNative_nativeSendRawKey(
    JNIEnv *env, jclass clazz, jint col, jint mask, jboolean pressed)
{
    (void)env;
    (void)clazz;
    if (!g_initialized) return;
    pthread_mutex_lock(&g_ctx.input_mutex);
    cybiko_input_set_raw_cell(&g_ctx.input, (int)col, (uint16_t)mask, (bool)pressed);
    pthread_mutex_unlock(&g_ctx.input_mutex);
}

JNIEXPORT jbyteArray JNICALL
Java_com_cybiko_emulator_CybikoNative_nativeGetNvram(JNIEnv *env, jclass clazz)
{
    (void)clazz;
    if (!g_initialized || !g_ctx.emu) return NULL;

    size_t len = 0;
    const uint8_t *nvram = cybiko_get_nvram(g_ctx.emu, &len);
    if (!nvram || len == 0) return NULL;

    jbyteArray result = (*env)->NewByteArray(env, (jsize)len);
    if (result) {
        (*env)->SetByteArrayRegion(env, result, 0, (jsize)len, (const jbyte *)nvram);
    }
    return result;
}

JNIEXPORT jboolean JNICALL
Java_com_cybiko_emulator_CybikoNative_nativeRenderToBitmap(
    JNIEnv *env, jclass clazz, jobject bitmap)
{
    (void)clazz;
    if (!g_initialized || !bitmap) return JNI_FALSE;

    AndroidBitmapInfo info;
    if (AndroidBitmap_getInfo(env, bitmap, &info) < 0) return JNI_FALSE;
    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) return JNI_FALSE;
    if (info.width != CYBIKO_LCD_WIDTH || info.height != CYBIKO_LCD_HEIGHT) return JNI_FALSE;

    void *pixels = NULL;
    if (AndroidBitmap_lockPixels(env, bitmap, &pixels) < 0) return JNI_FALSE;

    pthread_mutex_lock(&g_ctx.render_mutex);
    memcpy(pixels, g_ctx.frame_rgba, sizeof(g_ctx.frame_rgba));
    g_ctx.frame_ready = false;
    pthread_mutex_unlock(&g_ctx.render_mutex);

    AndroidBitmap_unlockPixels(env, bitmap);
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_cybiko_emulator_CybikoNative_nativeSetPalette(
    JNIEnv *env, jclass clazz, jint palette_mode)
{
    (void)env;
    (void)clazz;
    g_ctx.palette_mode = (palette_mode >= 0 && palette_mode <= 2) ? palette_mode : 0;
}

JNIEXPORT jfloat JNICALL
Java_com_cybiko_emulator_CybikoNative_nativeGetFps(JNIEnv *env, jclass clazz)
{
    (void)env;
    (void)clazz;
    return g_ctx.current_fps;
}

JNIEXPORT jfloat JNICALL
Java_com_cybiko_emulator_CybikoNative_nativeGetCoreMs(JNIEnv *env, jclass clazz)
{
    (void)env;
    (void)clazz;
    return g_ctx.avg_core_ms;
}
