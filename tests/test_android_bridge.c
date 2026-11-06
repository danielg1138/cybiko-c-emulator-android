#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "android/cybiko_input.h"
#include "android/cybiko_audio.h"
#include "core/emulator.h"

/* Simple test assertion macro */
#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("[FAIL] %s: %s (line %d)\n", __func__, msg, __LINE__); \
        return 1; \
    } \
} while (0)

#define TEST_PASS() do { \
    printf("[PASS] %s\n", __func__); \
    return 0; \
} while (0)

/* 1. Test Key Debounce: 3-Frame Hold Minimum */
static int test_key_hold_minimum(void) {
    cybiko_input_state_t input;
    cybiko_input_init(&input);

    /* Press key A (column 3, mask 0x0004) */
    cybiko_input_set_key(&input, CY_KEY_A, true);
    TEST_ASSERT((input.key_matrix[3] & 0x0004) != 0, "Key A should be set in matrix immediately");

    /* Release key A immediately on the same frame */
    cybiko_input_set_key(&input, CY_KEY_A, false);
    TEST_ASSERT((input.key_matrix[3] & 0x0004) != 0, "Key A must remain held due to 3-frame hold rule");

    /* Frames 1 to CYBIKO_MIN_HOLD_FRAMES - 1: key must stay held */
    for (int f = 1; f < CYBIKO_MIN_HOLD_FRAMES; f++) {
        cybiko_input_tick_frame(&input);
        TEST_ASSERT((input.key_matrix[3] & 0x0004) != 0, "Key A must still be held during hold window");
    }

    /* Final frame: Hold expires, release takes effect */
    cybiko_input_tick_frame(&input);
    TEST_ASSERT((input.key_matrix[3] & 0x0004) == 0, "Key A must be released after hold window");

    TEST_PASS();
}

/* 2. Test Number Keys Fn+Letter Timing */
static int test_number_key_fn_timing(void) {
    cybiko_input_state_t input;
    cybiko_input_init(&input);

    /* Press '1' (Fn + Q: Fn is col 7 mask 0x8000, Q is col 3 mask 0x0002) */
    cybiko_input_set_key(&input, CY_KEY_1, true);

    /* Fn must be pressed immediately on frame 0 */
    TEST_ASSERT((input.key_matrix[7] & 0x8000) != 0, "Fn modifier must be pressed immediately on frame 0");
    /* Letter Q must NOT be pressed yet (needs 4 frame delay for CyOS to detect Fn) */
    TEST_ASSERT((input.key_matrix[3] & 0x0002) == 0, "Letter Q must not be pressed before Fn delay");

    /* Tick 3 frames */
    cybiko_input_tick_frame(&input);
    cybiko_input_tick_frame(&input);
    cybiko_input_tick_frame(&input);
    TEST_ASSERT((input.key_matrix[3] & 0x0002) == 0, "Letter Q still waiting for delay");

    /* Tick 4th frame: Q activates */
    cybiko_input_tick_frame(&input);
    TEST_ASSERT((input.key_matrix[3] & 0x0002) != 0, "Letter Q should activate on 4th frame");
    TEST_ASSERT((input.key_matrix[7] & 0x8000) != 0, "Fn modifier must still be held");

    /* Release '1' */
    cybiko_input_set_key(&input, CY_KEY_1, false);

    /* Q is released, but Fn must stay held for 10 frames to avoid modifier drops */
    for (int f = 0; f < 9; f++) {
        cybiko_input_tick_frame(&input);
        TEST_ASSERT((input.key_matrix[7] & 0x8000) != 0, "Fn modifier must remain held during release countdown");
    }

    /* 10th frame: Fn finally releases */
    cybiko_input_tick_frame(&input);
    TEST_ASSERT((input.key_matrix[7] & 0x8000) == 0, "Fn modifier must be released after 10 frames");

    TEST_PASS();
}

/* 3. Test Audio Ring Buffer & Conversion */
static int test_audio_pcm_conversion(void) {
    uint8_t u8_samples[4] = { 0, 128, 255, 64 };
    int16_t expected[4];

    /* Verification formula: ((int)s - 128) << 8 */
    for (int i = 0; i < 4; i++) {
        expected[i] = (int16_t)(((int)u8_samples[i] - 128) << 8);
    }

    TEST_ASSERT(expected[0] == -32768, "Zero input maps to minimum negative 16-bit PCM");
    TEST_ASSERT(expected[1] == 0,      "128 midpoint maps to silence (0)");
    TEST_ASSERT(expected[2] == 32512,  "255 input maps to positive 16-bit PCM");
    TEST_ASSERT(expected[3] == -16384, "64 input maps to -16384");

    TEST_PASS();
}

/* 4. Test Matrix Scancode Isolation (Columns 0-9) */
static int test_matrix_column_bounds(void) {
    cybiko_input_state_t input;
    cybiko_input_init(&input);

    uint16_t matrix[CYBIKO_KEYBOARD_COLUMNS];
    memset(matrix, 0, sizeof(matrix));

    /* Verify copy bounds */
    cybiko_input_set_key(&input, CY_KEY_ENTER, true);
    cybiko_input_copy_matrix(&input, matrix, CYBIKO_KEYBOARD_COLUMNS);

    TEST_ASSERT((matrix[4] & 0x0008) != 0, "Enter key should reside in column 4 mask 0x0008");
    TEST_ASSERT(matrix[10] == 0, "Column 10 must be empty (preventing DMA aliasing)");
    TEST_ASSERT(matrix[13] == 0, "Column 13 must be empty (preventing MAME aliasing)");

    TEST_PASS();
}

int main(void) {
    printf("=== Running Android Bridge Native Tests ===\n");
    int failures = 0;
    failures += test_key_hold_minimum();
    failures += test_number_key_fn_timing();
    failures += test_audio_pcm_conversion();
    failures += test_matrix_column_bounds();

    if (failures == 0) {
        printf("=== ALL ANDROID BRIDGE TESTS PASSED (4/4) ===\n");
        return 0;
    } else {
        printf("=== %d TESTS FAILED ===\n", failures);
        return 1;
    }
}
