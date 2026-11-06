#include "cybiko_input.h"
#include <string.h>

typedef struct {
    int      key_id;
    int      col;
    uint16_t mask;
    bool     is_number;
} cy_key_def_t;

/* Key map table matching the verified Cybiko XT scancode matrix */
static const cy_key_def_t KEY_DEFS[] = {
    /* Letters Column 0 */
    { CY_KEY_M,         0, 0x0100, false },
    { CY_KEY_K,         0, 0x0800, false },
    { CY_KEY_I,         0, 0x1000, false },
    { CY_KEY_O,         0, 0x2000, false },
    { CY_KEY_COMMA,     0, 0x0400, false },
    { CY_KEY_L,         0, 0x4000, false },
    { CY_KEY_F7,        0, 0x0001, false },

    /* Letters Column 1 */
    { CY_KEY_F6,        1, 0x0001, false },
    { CY_KEY_G,         1, 0x0002, false },
    { CY_KEY_B,         1, 0x0004, false },
    { CY_KEY_N,         1, 0x0008, false },
    { CY_KEY_H,         1, 0x0010, false },
    { CY_KEY_Y,         1, 0x0020, false },
    { CY_KEY_U,         1, 0x0040, false },
    { CY_KEY_J,         1, 0x0080, false },

    /* Letters Column 2 */
    { CY_KEY_F5,        2, 0x0001, false },
    { CY_KEY_D,         2, 0x0100, false },
    { CY_KEY_C,         2, 0x0200, false },
    { CY_KEY_V,         2, 0x0800, false },
    { CY_KEY_F,         2, 0x1000, false },
    { CY_KEY_R,         2, 0x2000, false },
    { CY_KEY_T,         2, 0x4000, false },

    /* Letters Column 3 */
    { CY_KEY_F4,        3, 0x0001, false },
    { CY_KEY_Q,         3, 0x0002, false },
    { CY_KEY_A,         3, 0x0004, false },
    { CY_KEY_Z,         3, 0x0008, false },
    { CY_KEY_X,         3, 0x0010, false },
    { CY_KEY_S,         3, 0x0020, false },
    { CY_KEY_W,         3, 0x0040, false },
    { CY_KEY_E,         3, 0x0080, false },

    /* Control Column 4 */
    { CY_KEY_F3,        4, 0x0001, false },
    { CY_KEY_ENTER,     4, 0x0008, false },
    { CY_KEY_SELECT,    4, 0x0010, false },
    { CY_KEY_SPACE,     4, 0x0040, false },

    /* Navigation/Edit Column 5 */
    { CY_KEY_F2,        5, 0x0001, false },
    { CY_KEY_TAB,       5, 0x0080, false },
    { CY_KEY_DEL,       5, 0x0100, false },
    { CY_KEY_AS,        5, 0x0200, false },
    { CY_KEY_ESC,       5, 0x0400, false },

    /* D-pad Column 6 */
    { CY_KEY_F1,        6, 0x0001, false },
    { CY_KEY_UP,        6, 0x0800, false },
    { CY_KEY_RIGHT,     6, 0x1000, false },
    { CY_KEY_DOWN,      6, 0x2000, false },
    { CY_KEY_LEFT,      6, 0x4000, false },

    /* Modifiers */
    { CY_KEY_FN,        7, 0x8000, false },
    { CY_KEY_SHIFT,     8, 0x8000, false },

    /* Column 9 */
    { CY_KEY_HELP,      9, 0x0001, false },
    { CY_KEY_PERIOD,    9, 0x0002, false },
    { CY_KEY_SEMICOLON, 9, 0x0008, false },
    { CY_KEY_P,         9, 0x0010, false },

    /* Number Keys (Fn + letter) */
    { CY_KEY_1,         3, 0x0002, true },  /* Fn+Q */
    { CY_KEY_2,         3, 0x0040, true },  /* Fn+W */
    { CY_KEY_3,         3, 0x0080, true },  /* Fn+E */
    { CY_KEY_4,         2, 0x2000, true },  /* Fn+R */
    { CY_KEY_5,         2, 0x4000, true },  /* Fn+T */
    { CY_KEY_6,         1, 0x0020, true },  /* Fn+Y */
    { CY_KEY_7,         1, 0x0040, true },  /* Fn+U */
    { CY_KEY_8,         0, 0x1000, true },  /* Fn+I */
    { CY_KEY_9,         0, 0x2000, true },  /* Fn+O */
    { CY_KEY_0,         9, 0x0010, true },  /* Fn+P */
};

static const int NUM_KEY_DEFS = sizeof(KEY_DEFS) / sizeof(KEY_DEFS[0]);

void cybiko_input_init(cybiko_input_state_t *state)
{
    memset(state, 0, sizeof(*state));
}

static int find_held(cybiko_input_state_t *s, int col, uint16_t mask)
{
    for (int i = 0; i < s->held_count; i++) {
        if (s->held_col[i] == col && s->held_mask[i] == mask) return i;
    }
    return -1;
}

static void remove_held(cybiko_input_state_t *s, int idx)
{
    s->held_count--;
    if (idx < s->held_count) {
        s->held_col[idx]              = s->held_col[s->held_count];
        s->held_mask[idx]             = s->held_mask[s->held_count];
        s->held_frames[idx]           = s->held_frames[s->held_count];
        s->held_release_pending[idx]  = s->held_release_pending[s->held_count];
    }
}

static void press_key_with_hold(cybiko_input_state_t *s, int col, uint16_t mask, bool pressed)
{
    if (pressed) {
        s->key_matrix[col] |= mask;
        int idx = find_held(s, col, mask);
        if (idx >= 0) {
            s->held_frames[idx] = CYBIKO_MIN_HOLD_FRAMES;
            s->held_release_pending[idx] = false;
        } else if (s->held_count < CYBIKO_MAX_HELD_KEYS) {
            int n = s->held_count++;
            s->held_col[n]    = col;
            s->held_mask[n]   = mask;
            s->held_frames[n] = CYBIKO_MIN_HOLD_FRAMES;
            s->held_release_pending[n] = false;
        }
    } else {
        int idx = find_held(s, col, mask);
        if (idx >= 0 && s->held_frames[idx] > 0) {
            s->held_release_pending[idx] = true;
        } else {
            s->key_matrix[col] &= ~mask;
            if (idx >= 0) remove_held(s, idx);
        }
    }
}

static void queue_pending_letter(cybiko_input_state_t *s, int col, uint16_t mask, int delay)
{
    for (int i = 0; i < s->pending_count; i++) {
        if (s->pending_col[i] == col && s->pending_mask[i] == mask) {
            s->pending_delay[i] = delay;
            s->pending_auto_release[i] = false;
            return;
        }
    }
    if (s->pending_count < CYBIKO_MAX_PENDING_LETTERS) {
        int n = s->pending_count++;
        s->pending_col[n]   = col;
        s->pending_mask[n]  = mask;
        s->pending_delay[n] = delay;
        s->pending_auto_release[n] = false;
    }
}

static void mark_pending_auto_release(cybiko_input_state_t *s, int col, uint16_t mask)
{
    for (int i = 0; i < s->pending_count; i++) {
        if (s->pending_col[i] == col && s->pending_mask[i] == mask) {
            s->pending_auto_release[i] = true;
            return;
        }
    }
}

static bool is_pending(cybiko_input_state_t *s, int col, uint16_t mask)
{
    for (int i = 0; i < s->pending_count; i++) {
        if (s->pending_col[i] == col && s->pending_mask[i] == mask) return true;
    }
    return false;
}

static void set_fn_letter(cybiko_input_state_t *s, int col, uint16_t mask, bool pressed)
{
    if (pressed) {
        s->fn_held_count++;
        s->fn_release_countdown = 0;
        if (!s->fn_held) {
            s->key_matrix[7] |= 0x8000;  /* Press Fn first */
            s->fn_held = true;
            queue_pending_letter(s, col, mask, CYBIKO_FN_FIRST_DELAY);
        } else {
            queue_pending_letter(s, col, mask, CYBIKO_FN_NEXT_DELAY);
        }
    } else {
        if (is_pending(s, col, mask)) {
            mark_pending_auto_release(s, col, mask);
        } else {
            s->key_matrix[col] &= ~mask;
        }
        if (s->fn_held_count > 0) s->fn_held_count--;
        if (s->fn_held_count == 0 && s->fn_held) {
            s->fn_release_countdown = CYBIKO_FN_RELEASE_DELAY;
        }
    }
}

void cybiko_input_set_key(cybiko_input_state_t *state, int key_id, bool pressed)
{
    for (int i = 0; i < NUM_KEY_DEFS; i++) {
        if (KEY_DEFS[i].key_id == key_id) {
            if (KEY_DEFS[i].is_number) {
                set_fn_letter(state, KEY_DEFS[i].col, KEY_DEFS[i].mask, pressed);
            } else {
                press_key_with_hold(state, KEY_DEFS[i].col, KEY_DEFS[i].mask, pressed);
            }
            return;
        }
    }
}

void cybiko_input_set_raw_cell(cybiko_input_state_t *state, int col, uint16_t mask, bool pressed)
{
    if (col < 0 || col >= CYBIKO_KEYBOARD_COLUMNS) return;
    press_key_with_hold(state, col, mask, pressed);
}

void cybiko_input_tick_frame(cybiko_input_state_t *s)
{
    /* 1. Process minimum hold timers */
    for (int i = 0; i < s->held_count; i++) {
        if (s->held_frames[i] > 0) s->held_frames[i]--;
        if (s->held_frames[i] <= 0 && s->held_release_pending[i]) {
            s->key_matrix[s->held_col[i]] &= ~s->held_mask[i];
            remove_held(s, i);
            i--;
        }
    }

    /* 2. Process pending Fn+letter presses */
    for (int i = 0; i < s->pending_count; i++) {
        if (--s->pending_delay[i] <= 0) {
            int col = s->pending_col[i];
            uint16_t mask = s->pending_mask[i];
            bool auto_rel = s->pending_auto_release[i];

            s->key_matrix[col] |= mask;

            s->pending_count--;
            if (i < s->pending_count) {
                s->pending_col[i]          = s->pending_col[s->pending_count];
                s->pending_mask[i]         = s->pending_mask[s->pending_count];
                s->pending_delay[i]        = s->pending_delay[s->pending_count];
                s->pending_auto_release[i] = s->pending_auto_release[s->pending_count];
            }
            i--;

            if (auto_rel && s->held_count < CYBIKO_MAX_HELD_KEYS) {
                int n = s->held_count++;
                s->held_col[n]    = col;
                s->held_mask[n]   = mask;
                s->held_frames[n] = CYBIKO_MIN_HOLD_FRAMES;
                s->held_release_pending[n] = true;
            }
        }
    }

    /* 3. Delayed Fn release */
    if (s->fn_release_countdown > 0) {
        s->fn_release_countdown--;
        if (s->fn_release_countdown == 0 && s->fn_held) {
            s->key_matrix[7] &= ~0x8000;
            s->fn_held = false;
        }
    }
}

void cybiko_input_copy_matrix(const cybiko_input_state_t *state, uint16_t *dest, int count)
{
    int n = count < CYBIKO_KEYBOARD_COLUMNS ? count : CYBIKO_KEYBOARD_COLUMNS;
    memcpy(dest, state->key_matrix, (size_t)n * sizeof(uint16_t));
}
