#include "cybiko_audio.h"
#include <aaudio/AAudio.h>
#include <android/log.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define TAG "CybikoAudio"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

struct cybiko_audio {
    AAudioStream   *stream;
    int16_t         ring_buffer[CYBIKO_AUDIO_RING_BUFFER_SIZE];
    int             read_pos;
    int             write_pos;
    int             buffered_samples;
    pthread_mutex_t mutex;
    bool            running;
};

/* AAudio data callback invoked on the high-priority audio thread */
static aaudio_data_callback_result_t aaudio_callback(
    AAudioStream *stream,
    void *userData,
    void *audioData,
    int32_t numFrames)
{
    (void)stream;
    cybiko_audio_t *audio = (cybiko_audio_t *)userData;
    int16_t *output = (int16_t *)audioData;

    pthread_mutex_lock(&audio->mutex);
    int available = audio->buffered_samples;
    int to_copy = (available < numFrames) ? available : numFrames;

    for (int i = 0; i < to_copy; i++) {
        output[i] = audio->ring_buffer[audio->read_pos];
        audio->read_pos = (audio->read_pos + 1) % CYBIKO_AUDIO_RING_BUFFER_SIZE;
    }
    audio->buffered_samples -= to_copy;
    pthread_mutex_unlock(&audio->mutex);

    /* Fill remaining frames with silence if underrun occurred */
    if (to_copy < numFrames) {
        memset(&output[to_copy], 0, (size_t)(numFrames - to_copy) * sizeof(int16_t));
    }

    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static void aaudio_error_callback(
    AAudioStream *stream,
    void *userData,
    aaudio_result_t error)
{
    (void)stream;
    (void)userData;
    LOGE("AAudio error: %s (%d)", AAudio_convertResultToText(error), error);
}

cybiko_audio_t *cybiko_audio_create(void)
{
    cybiko_audio_t *audio = calloc(1, sizeof(cybiko_audio_t));
    if (!audio) return NULL;

    pthread_mutex_init(&audio->mutex, NULL);
    return audio;
}

bool cybiko_audio_start(cybiko_audio_t *audio)
{
    if (!audio) return false;
    if (audio->running) return true;

    AAudioStreamBuilder *builder = NULL;
    aaudio_result_t res = AAudio_createStreamBuilder(&builder);
    if (res != AAUDIO_OK) {
        LOGE("Failed to create AAudioStreamBuilder: %d", res);
        return false;
    }

    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setChannelCount(builder, 1);
    AAudioStreamBuilder_setSampleRate(builder, CYBIKO_AUDIO_SAMPLE_RATE);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setDataCallback(builder, aaudio_callback, audio);
    AAudioStreamBuilder_setErrorCallback(builder, aaudio_error_callback, audio);

    res = AAudioStreamBuilder_openStream(builder, &audio->stream);
    AAudioStreamBuilder_delete(builder);

    if (res != AAUDIO_OK) {
        LOGE("Failed to open AAudio stream: %s (%d)", AAudio_convertResultToText(res), res);
        return false;
    }

    res = AAudioStream_requestStart(audio->stream);
    if (res != AAUDIO_OK) {
        LOGE("Failed to start AAudio stream: %s (%d)", AAudio_convertResultToText(res), res);
        AAudioStream_close(audio->stream);
        audio->stream = NULL;
        return false;
    }

    audio->running = true;
    LOGD("AAudio stream started at %d Hz mono", CYBIKO_AUDIO_SAMPLE_RATE);
    return true;
}

void cybiko_audio_stop(cybiko_audio_t *audio)
{
    if (!audio || !audio->running) return;

    if (audio->stream) {
        AAudioStream_requestStop(audio->stream);
        AAudioStream_close(audio->stream);
        audio->stream = NULL;
    }
    audio->running = false;
    LOGD("AAudio stream stopped");
}

void cybiko_audio_push_u8(cybiko_audio_t *audio, const uint8_t *samples, int count)
{
    if (!audio || !audio->running || count <= 0) return;

    pthread_mutex_lock(&audio->mutex);
    for (int i = 0; i < count; i++) {
        /* Convert 8-bit unsigned mono (0..255) to 16-bit signed PCM (-32768..32767) */
        int16_t sample16 = (int16_t)(((int)samples[i] - 128) << 8);

        if (audio->buffered_samples < CYBIKO_AUDIO_RING_BUFFER_SIZE) {
            audio->ring_buffer[audio->write_pos] = sample16;
            audio->write_pos = (audio->write_pos + 1) % CYBIKO_AUDIO_RING_BUFFER_SIZE;
            audio->buffered_samples++;
        } else {
            /* Overflow drop */
            break;
        }
    }
    pthread_mutex_unlock(&audio->mutex);
}

void cybiko_audio_clear(cybiko_audio_t *audio)
{
    if (!audio) return;
    pthread_mutex_lock(&audio->mutex);
    audio->read_pos = 0;
    audio->write_pos = 0;
    audio->buffered_samples = 0;
    pthread_mutex_unlock(&audio->mutex);
}

void cybiko_audio_destroy(cybiko_audio_t *audio)
{
    if (!audio) return;
    cybiko_audio_stop(audio);
    pthread_mutex_destroy(&audio->mutex);
    free(audio);
}
