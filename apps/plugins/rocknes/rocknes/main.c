/* main.c - Rockbox plugin entry point for the NES emulator port.
 *
 * Core emulation (cpu6502/ppu/apu/mmu/mapper) is a lightly trimmed
 * copy of https://github.com/ObaraEmmanuel/NES (MIT licensed, see
 * README.md at the repo root for full attribution). The NSF player,
 * Android glue, and nametable debug mode were removed since they
 * don't apply to a Rockbox .nes-cartridge-only build. Everything
 * touching SDL was rewritten against the plugin API instead - see
 * gfx_rockbox.c, input_rockbox.c, audio_rockbox.c, and the modified
 * timers.c/controller.c/mapper.c/apu.c in core/.
 *
 * Usage: browse to a .nes file in the Rockbox file browser and open
 * it with this plugin (or register it as the default handler for
 * .nes in the plugin's configuration).
 */
#include "plugin.h"
#include "emulator.h"
#include "rocknes.h"
#include "utils.h"
#include <setjmp.h>

PLUGIN_HEADER

static Emulator emulator;

/* definitions for the quit()-replacement declared in utils.h */
jmp_buf rocknes_quit_jmp;
int rocknes_quit_code;

enum plugin_status plugin_start(const void* parameter)
{
    if (!parameter) {
        rb->splash(HZ * 3, "Play a .nes ROM file!");
        return PLUGIN_OK;
    }

    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_clear_display();
    rb->lcd_puts(0, 0, "Loading ROM...");
    rb->lcd_update();

    /* ignore backlight timeout while playing, same as other Rockbox
     * emulator plugins - nobody wants the screen dimming mid-game */
    backlight_ignore_timeout();

    /* core code (mapper.c's load_file) calls quit() on unrecoverable
     * errors like a bad/unsupported ROM; quit() longjmps here instead
     * of calling libc exit(), which wouldn't make sense in a plugin */
    if (setjmp(rocknes_quit_jmp)) {
        rb->splash(HZ * 2, "Failed to load ROM");
        backlight_use_settings();
        return PLUGIN_ERROR;
    }

    init_emulator(&emulator, (const char*)parameter);

    /* Audio disabled (see AUDIO_ENABLED in apu.c) -- skip engaging
     * Rockbox's mixer channel entirely rather than init it and never
     * feed it. */
#if 0
    rocknes_audio_init();
#endif
    rb->lcd_clear_display();

    run_emulator(&emulator);

#if 0
    rocknes_audio_close();
#endif
    free_emulator(&emulator);

    backlight_use_settings();

    return PLUGIN_OK;
}
