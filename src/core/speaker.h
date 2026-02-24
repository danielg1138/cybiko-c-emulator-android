#ifndef CYBIKO_SPEAKER_H
#define CYBIKO_SPEAKER_H
#include "types.h"

#define SPEAKER_SAMPLE_RATE 44100
#define SPEAKER_BUFFER_SIZE 2048

typedef struct {
    int      current_level;
    double   cycles_per_sample;
    double   cycle_fraction;
    uint8_t  buffer[SPEAKER_BUFFER_SIZE];
    int      buffer_pos;
} speaker_t;

void speaker_init(speaker_t *spk, int clock_hz);
void speaker_set_level(speaker_t *spk, int level);
int  speaker_generate_samples(speaker_t *spk, int cpu_cycles, uint8_t *out, int out_size);

#endif
