/*
 * H8S 16-bit timer (TPU - Timer Pulse Unit) emulation.
 *
 * Each channel has TCR, TMDR, TIOR, TIER, TSR, TCNT (16-bit), TGRA, TGRB.
 * Clock source mapping is per-channel (from MAME h8s2319.cpp).
 */
#include "core/timer16.h"
#include "core/h8s_cpu.h"

/* TIER bits */
#define TIER_TGIEA 0x01
#define TIER_TGIEB 0x02
#define TIER_OVIE  0x10

/* TSR bits */
#define TSR_TGFA   0x01
#define TSR_TGFB   0x02
#define TSR_OVF    0x10

/* Per-channel clock divisor tables (from MAME h8s2319.cpp) */
static const int ch0_divisors[8] = {1, 4, 16, 64, 0, 0, 0, 0};
static const int ch1_divisors[8] = {1, 4, 16, 64, 0, 0, 256, 0};
static const int ch2_divisors[8] = {1, 4, 16, 64, 0, 0, 0, 1024};
static const int ch3_divisors[8] = {1, 4, 16, 64, 0, 1024, 256, 4096};
static const int ch4_divisors[8] = {1, 4, 16, 64, 0, 0, 1024, 0};
static const int ch5_divisors[8] = {1, 4, 16, 64, 0, 0, 256, 0};

static const int *channel_clock_divisors[6] = {
    ch0_divisors, ch1_divisors, ch2_divisors,
    ch3_divisors, ch4_divisors, ch5_divisors
};

static void update_cached_divisor(timer16_t *t) {
    if (t->enabled) {
        t->cached_divisor = t->clock_divisors[t->tcr & 0x07];
    } else {
        t->cached_divisor = 0;
    }
}

void timer16_init(timer16_t *t, int channel, int tgr_count, int base_vector, h8s_cpu_t *cpu) {
    t->tcr   = 0;
    t->tmdr  = 0;
    t->tior  = 0;
    t->tier  = 0;
    t->tsr   = 0;
    t->tcnt  = 0;
    t->tgra  = 0xFFFF;
    t->tgrb  = 0xFFFF;
    t->prescale_counter = 0;
    t->cached_divisor   = 0;
    t->enabled = false;
    t->channel   = channel;
    t->tgr_count = tgr_count;
    t->vec_tgia = base_vector;
    t->vec_tgib = base_vector + 1;
    t->vec_ovf  = base_vector + tgr_count;
    t->cpu = cpu;

    if (channel >= 0 && channel < 6) {
        t->clock_divisors = channel_clock_divisors[channel];
    } else {
        t->clock_divisors = ch0_divisors;
    }
}

uint8_t timer16_read8(const timer16_t *t, int reg) {
    switch (reg) {
    case 0:   return t->tcr;
    case 1:   return t->tmdr;
    case 2:   return t->tior;
    case 4:   return t->tier;
    case 5:   return t->tsr;
    case 6:   return (uint8_t)(t->tcnt >> 8);
    case 7:   return (uint8_t)(t->tcnt & 0xFF);
    case 8:   return (uint8_t)(t->tgra >> 8);
    case 9:   return (uint8_t)(t->tgra & 0xFF);
    case 0xA: return (uint8_t)(t->tgrb >> 8);
    case 0xB: return (uint8_t)(t->tgrb & 0xFF);
    default:  return 0;
    }
}

uint16_t timer16_read16(const timer16_t *t, int reg) {
    switch (reg) {
    case 6:  return t->tcnt;
    case 8:  return t->tgra;
    case 0xA: return t->tgrb;
    default:
        return ((uint16_t)timer16_read8(t, reg) << 8) |
               timer16_read8(t, reg + 1);
    }
}

void timer16_write8(timer16_t *t, int reg, uint8_t value) {
    switch (reg) {
    case 0:
        t->tcr = value;
        update_cached_divisor(t);
        break;
    case 1:
        t->tmdr = value;
        break;
    case 2:
        t->tior = value;
        break;
    case 4:
        t->tier = value;
        break;
    case 5:
        /* Flags: write-0-to-clear for TGFA, TGFB, OVF */
        t->tsr = t->tsr & (value | (uint8_t)~(TSR_TGFA | TSR_TGFB | TSR_OVF));
        break;
    case 6:
        t->tcnt = ((uint16_t)(value) << 8) | (t->tcnt & 0xFF);
        break;
    case 7:
        t->tcnt = (t->tcnt & 0xFF00) | value;
        break;
    case 8:
        t->tgra = ((uint16_t)(value) << 8) | (t->tgra & 0xFF);
        break;
    case 9:
        t->tgra = (t->tgra & 0xFF00) | value;
        break;
    case 0xA:
        t->tgrb = ((uint16_t)(value) << 8) | (t->tgrb & 0xFF);
        break;
    case 0xB:
        t->tgrb = (t->tgrb & 0xFF00) | value;
        break;
    default:
        break;
    }
}

void timer16_write16(timer16_t *t, int reg, uint16_t value) {
    switch (reg) {
    case 6:
        t->tcnt = value;
        break;
    case 8:
        t->tgra = value;
        break;
    case 0xA:
        t->tgrb = value;
        break;
    default:
        timer16_write8(t, reg, (uint8_t)(value >> 8));
        timer16_write8(t, reg + 1, (uint8_t)(value & 0xFF));
        break;
    }
}

void timer16_set_enabled(timer16_t *t, bool enabled) {
    t->enabled = enabled;
    update_cached_divisor(t);
}

void timer16_tick(timer16_t *t) {
    int divisor = t->cached_divisor;
    if (divisor == 0) return; /* Stopped, external, or disabled */

    t->prescale_counter++;
    if (t->prescale_counter < divisor) return;
    t->prescale_counter = 0;

    uint16_t prev_tcnt = t->tcnt;
    t->tcnt = (t->tcnt + 1) & 0xFFFF;

    /* Compare match A */
    if (t->tcnt == t->tgra) {
        bool was_set = (t->tsr & TSR_TGFA) != 0;
        t->tsr |= TSR_TGFA;
        if (!was_set && (t->tier & TIER_TGIEA)) {
            h8s_cpu_request_interrupt(t->cpu, t->vec_tgia);
        }
        /* Clear on compare match A if configured */
        int clear_mode = (t->tcr >> 5) & 0x03;
        if (clear_mode == 1) t->tcnt = 0;
    }

    /* Compare match B */
    if (t->tcnt == t->tgrb) {
        bool was_set = (t->tsr & TSR_TGFB) != 0;
        t->tsr |= TSR_TGFB;
        if (!was_set && (t->tier & TIER_TGIEB)) {
            h8s_cpu_request_interrupt(t->cpu, t->vec_tgib);
        }
        int clear_mode = (t->tcr >> 5) & 0x03;
        if (clear_mode == 2) t->tcnt = 0;
    }

    /* Overflow (when counter wraps from 0xFFFF to 0) */
    if (t->tcnt == 0 && prev_tcnt == 0xFFFF) {
        bool was_set = (t->tsr & TSR_OVF) != 0;
        t->tsr |= TSR_OVF;
        if (!was_set && (t->tier & TIER_OVIE)) {
            h8s_cpu_request_interrupt(t->cpu, t->vec_ovf);
        }
    }
}

bool timer16_is_running(const timer16_t *t) {
    return t->enabled && t->clock_divisors[t->tcr & 0x07] != 0;
}
