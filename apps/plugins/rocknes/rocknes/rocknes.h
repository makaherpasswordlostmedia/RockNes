/* rocknes.h - Rockbox plugin glue for the NES emulator core
 *
 * This header ties the emulator core (cpu6502/ppu/apu/mmu/mapper, taken
 * from https://github.com/ObaraEmmanuel/NES, MIT licensed) to the Rockbox
 * plugin API. It replaces the SDL-based GraphicsContext used by the
 * upstream project.
 */
#ifndef __ROCKNES_H__
#define __ROCKNES_H__

#include "plugin.h"
#include <stdint.h>

/* NES native resolution - never changes, PPU always renders this */
#define NES_WIDTH  256
#define NES_HEIGHT 240

/* iPod Mini 2G: LCD_WIDTH=138 LCD_HEIGHT=110 LCD_DEPTH=2 (4 grey levels).
 * We scale down and dither into a 1 byte-per-pixel 2bpp-packed buffer
 * that lcd_update_rect understands via the greyscale plugin lib, OR we
 * go through rb->lcd_* generic calls. See gfx_rockbox.c. */

/* Replaces upstream GraphicsContext. No SDL types anywhere. */
typedef struct GraphicsContext {
    int width;          /* NES logical width  (256) */
    int height;         /* NES logical height (240) */
    int screen_width;   /* physical LCD width  */
    int screen_height;  /* physical LCD height */
    int out_w, out_h;   /* scaled output size that fits the LCD */
    int off_x, off_y;   /* centering offset on screen */
} GraphicsContext;

/* Called once at startup / teardown */
void get_graphics_context(GraphicsContext* ctx);
void free_graphics(GraphicsContext* ctx);

/* Called once per frame with the PPU's ARGB8888 256x240 buffer */
void render_graphics(GraphicsContext* g_ctx, const uint32_t* buffer);

/* ---- Audio ---- */
void rocknes_audio_init(void);
void rocknes_audio_close(void);
/* Called by apu.c to push a rendered block of signed 16-bit samples */
void rocknes_audio_submit(const int16_t* samples, size_t count);

/* ---- Input ---- */
/* Bit layout matches src/controller.h KeyPad enum from upstream core */
uint16_t rocknes_poll_input(void);

/* returns 1 if the user asked to quit (menu -> exit) */
int rocknes_should_exit(void);

#endif
