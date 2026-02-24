/*
 * 1-bit speaker audio output.
 *
 * The Cybiko speaker is driven by a single digital pin (Port 1 bit 3).
 * This module converts pin level transitions into PCM audio samples.
 */
#include "core/speaker.h"
#include <string.h>

void speaker_init(speaker_t *spk, int clock_hz) {
    memset(spk, 0, sizeof(*spk));
    spk->cycles_per_sample = (double)clock_hz / SPEAKER_SAMPLE_RATE;
    spk->cycle_fraction = 0.0;
    spk->current_level = 0;
    spk->buffer_pos = 0;
}

void speaker_set_level(speaker_t *spk, int level) {
    spk->current_level = level;
}

int speaker_generate_samples(speaker_t *spk, int cpu_cycles, uint8_t *out, int out_size) {
    spk->cycle_fraction += cpu_cycles;
    int samples_to_write = (int)(spk->cycle_fraction / spk->cycles_per_sample);
    spk->cycle_fraction -= samples_to_write * spk->cycles_per_sample;

    uint8_t sample = (spk->current_level == 0) ? 96 : 160;

    /* Clamp to output buffer size */
    if (samples_to_write > out_size) {
        samples_to_write = out_size;
    }

    for (int i = 0; i < samples_to_write; i++) {
        out[i] = sample;
    }

    return samples_to_write;
}
