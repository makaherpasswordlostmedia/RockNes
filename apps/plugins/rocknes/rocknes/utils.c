#include "utils.h"
#include "plugin.h"
#include <stdarg.h>
#include <string.h>
#include <setjmp.h>

extern const struct plugin_api* rb;

#ifndef PI
# define PI 3.14159265358979323846264338327950288
#endif

/* Upstream's quit() calls libc exit(), which has no sensible meaning
 * inside a Rockbox plugin (there's no process to terminate - it would
 * either no-op or crash depending on target). Instead we longjmp back
 * to a point set up in main.c's plugin_start(), which then reports
 * the failure and returns PLUGIN_ERROR cleanly. */
jmp_buf rocknes_quit_jmp;
int rocknes_quit_code;

void quit(int code) {
    rocknes_quit_code = code;
    longjmp(rocknes_quit_jmp, 1);
}

void rb_printf(const char* fmt, ...) {
#ifdef DEBUGGING_ENABLED
    char buf[128];
    va_list args;
    va_start(args, fmt);
    rb->vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    rb->splashf(HZ, "%s", buf);
#else
    (void)fmt;
#endif
}

void LOG(enum LogLevel logLevel, const char* fmt, ...) {
    if (TRACER) return;
    if (logLevel < LOGLEVEL) return;
#ifdef DEBUGGING_ENABLED
    char buf[128];
    va_list args;
    va_start(args, fmt);
    rb->vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    rb->splashf(HZ, "%s", buf);
#else
    (void)fmt;
#endif
}

void to_pixel_format(const uint32_t* in, uint32_t* out, size_t size, ColorFormat format) {
    for (size_t i = 0; i < size; i++) {
        switch (format) {
            case ARGB8888:
                out[i] = in[i];
                break;
            case ABGR8888: {
                uint32_t px = in[i];
                uint32_t a = (px >> 24) & 0xff;
                uint32_t r = (px >> 16) & 0xff;
                uint32_t g = (px >> 8)  & 0xff;
                uint32_t b =  px        & 0xff;
                out[i] = (a << 24) | (b << 16) | (g << 8) | r;
                break;
            }
        }
    }
}

uint64_t next_power_of_2(uint64_t num) {
    if (num == 0) return 1;
    num--;
    num |= num >> 1;
    num |= num >> 2;
    num |= num >> 4;
    num |= num >> 8;
    num |= num >> 16;
    num |= num >> 32;
    return num + 1;
}

char *get_file_name(char *path) {
    char* slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

/* fft() is only used by upstream's NSF visualizer, which this port
 * doesn't include (see emulator.h) - stub kept only so nothing fails
 * to link if some path still references it. */
void fft(complx *v, int n, complx *tmp) {
    (void)v; (void)n; (void)tmp;
}
