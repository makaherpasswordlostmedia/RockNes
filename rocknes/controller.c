#include "controller.h"

void init_joypad(struct JoyPad* joyPad, uint8_t player){
    joyPad->status = 0;
    joyPad->reg = 0;
    joyPad->player = player;
}

uint8_t read_joypad(struct JoyPad* joyPad){
    uint8_t val = joyPad->reg & 1;
    joyPad->reg >>= 1;
    // refill BIT 7 with 1
    joyPad->reg |= 0x80;
    return val;
}

/* Rockbox has no discrete key-up/key-down event stream reaching this
 * layer (that's polled once per frame in input_rockbox.c); we're just
 * given the current NES_* bitmask directly and latch it. Turbo isn't
 * wired up on the Mini 2G control scheme (no spare buttons), so
 * TURBO_A/TURBO_B bits are simply never set upstream and this is a
 * straight passthrough. */
void update_joypad(struct JoyPad* joyPad, uint16_t status){
    joyPad->status = status;
}

void turbo_trigger(struct JoyPad* joyPad){
    // toggle BUTTON_A AND BUTTON_B if TURBO_A and TURBO_B are set respectively
    joyPad->status ^= joyPad->status >> 8;
}
