#ifndef CYBIKO_ANDROID_INPUT_H
#define CYBIKO_ANDROID_INPUT_H

#include <stdint.h>
#include <stdbool.h>
#include "../core/emulator.h"

#define CYBIKO_MAX_HELD_KEYS       16
#define CYBIKO_MAX_PENDING_LETTERS 10
#define CYBIKO_MIN_HOLD_FRAMES     6
#define CYBIKO_FN_FIRST_DELAY      4
#define CYBIKO_FN_NEXT_DELAY       3
#define CYBIKO_FN_RELEASE_DELAY   10

typedef struct {
    uint16_t key_matrix[CYBIKO_KEYBOARD_COLUMNS];

    /* Minimum hold: keys stay pressed for at least CYBIKO_MIN_HOLD_FRAMES */
    int      held_col[CYBIKO_MAX_HELD_KEYS];
    uint16_t held_mask[CYBIKO_MAX_HELD_KEYS];
    int      held_frames[CYBIKO_MAX_HELD_KEYS];
    bool     held_release_pending[CYBIKO_MAX_HELD_KEYS];
    int      held_count;

    /* Fn+letter for number keys: Fn pressed first, letter after delay */
    int      pending_col[CYBIKO_MAX_PENDING_LETTERS];
    uint16_t pending_mask[CYBIKO_MAX_PENDING_LETTERS];
    int      pending_delay[CYBIKO_MAX_PENDING_LETTERS];
    bool     pending_auto_release[CYBIKO_MAX_PENDING_LETTERS];
    int      pending_count;
    int      fn_held_count;
    bool     fn_held;
    int      fn_release_countdown;
} cybiko_input_state_t;

/* Standard Cybiko key IDs passed from Kotlin/Java UI */
enum {
    CY_KEY_A = 1, CY_KEY_B, CY_KEY_C, CY_KEY_D, CY_KEY_E, CY_KEY_F, CY_KEY_G,
    CY_KEY_H, CY_KEY_I, CY_KEY_J, CY_KEY_K, CY_KEY_L, CY_KEY_M, CY_KEY_N,
    CY_KEY_O, CY_KEY_P, CY_KEY_Q, CY_KEY_R, CY_KEY_S, CY_KEY_T, CY_KEY_U,
    CY_KEY_V, CY_KEY_W, CY_KEY_X, CY_KEY_Y, CY_KEY_Z,
    CY_KEY_0, CY_KEY_1, CY_KEY_2, CY_KEY_3, CY_KEY_4,
    CY_KEY_5, CY_KEY_6, CY_KEY_7, CY_KEY_8, CY_KEY_9,
    CY_KEY_UP, CY_KEY_DOWN, CY_KEY_LEFT, CY_KEY_RIGHT,
    CY_KEY_SELECT, CY_KEY_ESC, CY_KEY_DEL, CY_KEY_ENTER, CY_KEY_SPACE, CY_KEY_TAB,
    CY_KEY_AS, CY_KEY_HELP, CY_KEY_PERIOD, CY_KEY_SEMICOLON, CY_KEY_COMMA,
    CY_KEY_FN, CY_KEY_SHIFT,
    CY_KEY_F1, CY_KEY_F2, CY_KEY_F3, CY_KEY_F4, CY_KEY_F5, CY_KEY_F6, CY_KEY_F7
};

void cybiko_input_init(cybiko_input_state_t *state);
void cybiko_input_set_key(cybiko_input_state_t *state, int key_id, bool pressed);
void cybiko_input_set_raw_cell(cybiko_input_state_t *state, int col, uint16_t mask, bool pressed);
void cybiko_input_tick_frame(cybiko_input_state_t *state);
void cybiko_input_copy_matrix(const cybiko_input_state_t *state, uint16_t *dest, int count);

#endif /* CYBIKO_ANDROID_INPUT_H */
