/* gfx_rockbox.c - replaces upstream gfx.c (which was pure SDL).
 *
 * Target: iPod Mini 2G. LCD_WIDTH=138 LCD_HEIGHT=110 LCD_DEPTH=2
 * (4 grey levels, HORIZONTAL_PACKING -> fb_data is 1 byte per pixel,
 * value range 0..3, native Rockbox handles the bit packing for us).
 *
 * We nearest-neighbour downscale the NES's native 256x240 ARGB8888
 * frame to fit the physical screen, and apply an 8x8 ordered (Bayer)
 * dither while converting to 4 grey levels. Ordered dither is used
 * instead of Floyd-Steinberg because it's branch-light, needs no
 * error buffer, and is cheap enough for an 11MHz ARM7TDMI to redo
 * every frame.
 */
#include "rocknes.h"
#include "plugin.h"

extern const struct plugin_api* rb;

/* 8x8 Bayer dither matrix, values 0..63 */
static const uint8_t bayer8x8[8][8] = {
    { 0,32, 8,40, 2,34,10,42},
    {48,16,56,24,50,18,58,26},
    {12,44, 4,36,14,46, 6,38},
    {60,28,52,20,62,30,54,22},
    { 3,35,11,43, 1,33, 9,41},
    {51,19,59,27,49,17,57,25},
    {15,47, 7,39,13,45, 5,37},
    {63,31,55,23,61,29,53,21}
};

void get_graphics_context(GraphicsContext* ctx)
{
    ctx->width  = NES_WIDTH;
    ctx->height = NES_HEIGHT;
    ctx->screen_width  = LCD_WIDTH;
    ctx->screen_height = LCD_HEIGHT;

    /* fit NES 256x240 (4:3.75 ~= 4:3.75) into the LCD preserving aspect,
     * then centre it. iPod Mini 2G screen is 138x110 (~5:4). */
    int scale_w_num = LCD_WIDTH,  scale_w_den = NES_WIDTH;
    int scale_h_num = LCD_HEIGHT, scale_h_den = NES_HEIGHT;

    /* compare scale_w vs scale_h as fractions without floats:
     * scale_w_num/scale_w_den  vs  scale_h_num/scale_h_den */
    if (scale_w_num * scale_h_den <= scale_h_num * scale_w_den) {
        ctx->out_w = LCD_WIDTH;
        ctx->out_h = (NES_HEIGHT * LCD_WIDTH) / NES_WIDTH;
    } else {
        ctx->out_h = LCD_HEIGHT;
        ctx->out_w = (NES_WIDTH * LCD_HEIGHT) / NES_HEIGHT;
    }
    if (ctx->out_w > LCD_WIDTH)  ctx->out_w = LCD_WIDTH;
    if (ctx->out_h > LCD_HEIGHT) ctx->out_h = LCD_HEIGHT;

    ctx->off_x = (LCD_WIDTH  - ctx->out_w) / 2;
    ctx->off_y = (LCD_HEIGHT - ctx->out_h) / 2;
}

void free_graphics(GraphicsContext* ctx)
{
    (void)ctx;
    rb->lcd_clear_display();
    rb->lcd_update();
}

/* extract perceptual luminance from ARGB8888 (upstream palette is opaque,
 * alpha is always 0xff, so we can ignore it) */
static inline uint8_t argb_to_luma(uint32_t px)
{
    uint8_t r = (px >> 16) & 0xff;
    uint8_t g = (px >> 8)  & 0xff;
    uint8_t b =  px        & 0xff;
    /* fixed point luma: 0.299R + 0.587G + 0.114B, scaled by 256 */
    return (uint8_t)((77 * r + 150 * g + 29 * b) >> 8);
}

void render_graphics(GraphicsContext* g_ctx, const uint32_t* buffer)
{
    /* There is no simple rb->lcd_framebuffer accessor - the real API
     * (see Rockboy's sys_rockbox.c) goes through the main screen's
     * current viewport. Cached after first call since the viewport
     * doesn't move during emulation. */
    static fb_data* fb = NULL;
    if (!fb) {
        struct viewport* vp = *(rb->screens[SCREEN_MAIN]->current_viewport);
        fb = vp->buffer->fb_ptr;
    }

    int out_w = g_ctx->out_w;
    int out_h = g_ctx->out_h;

    /* fixed-point (16.16) source-step for nearest-neighbour scaling */
    uint32_t x_step = (NES_WIDTH  << 16) / out_w;
    uint32_t y_step = (NES_HEIGHT << 16) / out_h;

    uint32_t src_y = 0;
    for (int y = 0; y < out_h; y++, src_y += y_step) {
        int sy = src_y >> 16;
        const uint32_t* row = buffer + sy * NES_WIDTH;
        fb_data* dst_row = fb + (g_ctx->off_y + y) * LCD_WIDTH + g_ctx->off_x;
        const uint8_t* dither_row = bayer8x8[y & 7];

        uint32_t src_x = 0;
        for (int x = 0; x < out_w; x++, src_x += x_step) {
            int sx = src_x >> 16;
            uint8_t luma = argb_to_luma(row[sx]);

            /* ordered dither into 4 levels (0..3): compare luma against
             * threshold offset by the Bayer matrix, quantize to nearest
             * of 4 buckets with dithered rounding */
            int level = (luma * 4 + dither_row[x & 7] - 32) >> 8;
            if (level < 0) level = 0;
            if (level > 3) level = 3;

            /* Rockbox greyscale convention: 0 = black, max = white.
             * NES palette luma is standard video luma so this maps
             * directly; invert here if colours look reversed on device. */
            dst_row[x] = (fb_data)level;
        }
    }

    rb->lcd_update_rect(g_ctx->off_x, g_ctx->off_y, out_w, out_h);
}
