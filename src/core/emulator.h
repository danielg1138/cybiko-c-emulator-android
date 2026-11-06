#ifndef CYBIKO_EMULATOR_H
#define CYBIKO_EMULATOR_H
#include "types.h"
#include "../hal/hal.h"

typedef struct cybiko_emu cybiko_emu_t;

cybiko_emu_t *cybiko_create(const cybiko_hal_t *hal);
void          cybiko_destroy(cybiko_emu_t *emu);
bool cybiko_load_boot_rom(cybiko_emu_t *emu, const uint8_t *data, size_t len);
bool cybiko_load_flash_rom(cybiko_emu_t *emu, const uint8_t *data, size_t len);
bool cybiko_load_nvram(cybiko_emu_t *emu, const uint8_t *data, size_t len);
void cybiko_reset(cybiko_emu_t *emu);
void cybiko_run_frame(cybiko_emu_t *emu);
bool cybiko_is_running(const cybiko_emu_t *emu);
const uint8_t *cybiko_get_nvram(const cybiko_emu_t *emu, size_t *len);

typedef enum {
    CYBIKO_SPEED_AUTHENTIC = 0,  /* 1.0x Authentic H8S speed (CPI = 4) */
    CYBIKO_SPEED_FAST      = 1,  /* 2.0x Fast Forward (CPI = 2) */
    CYBIKO_SPEED_TURBO     = 2,  /* 4.0x Turbo / Uncapped (CPI = 1) */
    CYBIKO_SPEED_CLASSIC   = 3,  /* Cybiko Classic 11.0592 MHz (CPI = 6) */
    CYBIKO_SPEED_COUNT     = 4
} cybiko_speed_mode_t;

void cybiko_set_speed_mode(cybiko_emu_t *emu, cybiko_speed_mode_t mode);
cybiko_speed_mode_t cybiko_get_speed_mode(const cybiko_emu_t *emu);
void cybiko_set_cpi(cybiko_emu_t *emu, int cpi);
int  cybiko_get_cpi(const cybiko_emu_t *emu);

#define CYBIKO_LCD_WIDTH   160
#define CYBIKO_LCD_HEIGHT  100
#define CYBIKO_CLOCK_HZ    18432000
#define CYBIKO_FPS         60
#define CYBIKO_CYCLES_PER_FRAME (CYBIKO_CLOCK_HZ / CYBIKO_FPS)
#define CYBIKO_KEYBOARD_COLUMNS 15  /* total matrix entries (HAL copies all) */
#define CYBIKO_KEYBOARD_SCAN_COLS 10 /* columns scanned by keyboard_read (0-9) */

#endif
