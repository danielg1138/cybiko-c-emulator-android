#ifndef CYBIKO_ANDROID_AUDIO_H
#define CYBIKO_ANDROID_AUDIO_H

#include <stdint.h>
#include <stdbool.h>

#define CYBIKO_AUDIO_SAMPLE_RATE 48000
#define CYBIKO_AUDIO_RING_BUFFER_SIZE (48000 * 2) /* 2 seconds buffer */

typedef struct cybiko_audio cybiko_audio_t;

cybiko_audio_t *cybiko_audio_create(void);
void cybiko_audio_destroy(cybiko_audio_t *audio);
bool cybiko_audio_start(cybiko_audio_t *audio);
void cybiko_audio_stop(cybiko_audio_t *audio);
void cybiko_audio_push_u8(cybiko_audio_t *audio, const uint8_t *samples, int count);
void cybiko_audio_clear(cybiko_audio_t *audio);

#endif /* CYBIKO_ANDROID_AUDIO_H */
