#pragma once

#include <stdint.h>

/* NES_ prefix used throughout to avoid clashing with Rockbox's own
 * BUTTON_LEFT / BUTTON_A / BUTTON_SELECT etc. from plugin.h */
typedef enum KeyPad{
    NES_TURBO_B     = 1 << 9,
    NES_TURBO_A     = 1 << 8,
    NES_RIGHT       = 1 << 7,
    NES_LEFT        = 1 << 6,
    NES_DOWN        = 1 << 5,
    NES_UP          = 1 << 4,
    NES_START       = 1 << 3,
    NES_SELECT      = 1 << 2,
    NES_BUTTON_B    = 1 << 1,
    NES_BUTTON_A    = 1
} KeyPad;

typedef struct JoyPad{
    uint16_t status;
    uint8_t reg;
    uint8_t player;
} JoyPad;


void init_joypad(struct JoyPad* joyPad, uint8_t player);
uint8_t read_joypad(struct JoyPad* joyPad);
/* Rockbox variant: caller passes in the already-polled NES_* bitmask
 * (built in input_rockbox.c from rb->button_get()) instead of an
 * SDL_Event*, since there is no SDL event queue on this platform. */
void update_joypad(struct JoyPad* joyPad, uint16_t status);
void turbo_trigger(struct JoyPad* joyPad);
