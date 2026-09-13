/* input_rockbox.c - replaces upstream keyboard_mapper()/update_joypad()
 * (which read SDL_Event) with Rockbox's button_get() polling model.
 *
 * iPod Mini 2G uses IPOD_4G_PAD keymap: scrollwheel ring, centre
 * SELECT button, PLAY/MENU buttons. There is no analog d-pad or
 * enough spare buttons for a full NES pad, so:
 *   scrollwheel quadrants -> D-pad (UP/DOWN/LEFT/RIGHT)
 *   SELECT (centre click)  -> NES A
 *   PLAY                   -> NES B
 *   PLAY + SELECT together -> NES START
 *   MENU (hold)             -> exit emulator
 */
#include "rocknes.h"
#include "controller.h"
#include "plugin.h"

extern const struct plugin_api* rb;

static int quit_requested = 0;

int rocknes_should_exit(void)
{
    return quit_requested;
}

uint16_t rocknes_poll_input(void)
{
    uint16_t status = 0;
    int btn = rb->button_get(false);

    if (btn & BUTTON_MENU)   { quit_requested = 1; }
    if (btn & BUTTON_LEFT)   status |= (uint16_t)NES_LEFT;
    if (btn & BUTTON_RIGHT)  status |= (uint16_t)NES_RIGHT;
    if (btn & BUTTON_UP)     status |= (uint16_t)NES_UP;
    if (btn & BUTTON_DOWN)   status |= (uint16_t)NES_DOWN;
    if (btn & BUTTON_SELECT) status |= (uint16_t)NES_BUTTON_A;
    if (btn & BUTTON_PLAY)   status |= (uint16_t)NES_BUTTON_B;

    if ((btn & BUTTON_PLAY) && (btn & BUTTON_SELECT)) {
        status |= (uint16_t)NES_START;
    }

    return status;
}
