#pragma once

#include <stdint.h>

/* Rockbox's tick resolution is 10ms (HZ=100, see firmware/kernel/include/
 * tick.h) versus the sub-millisecond clock_gettime/QueryPerformanceCounter
 * upstream used. That's coarse relative to a 16.67ms NTSC frame budget,
 * but it's what the platform gives us, and it's precise enough to pace
 * frames without drifting badly over a play session. */
typedef struct Timer{
    long start_tick;
    long period_ticks; /* target frame period, in Rockbox ticks */
    long last_diff_ticks;
} Timer;

/* period is in nanoseconds for API compatibility with the emulator core's
 * PERIOD constant (1e9/60 etc); converted to ticks internally. */
void init_timer(Timer* timer, uint64_t period_ns);
void mark_start(Timer* timer);
void mark_end(Timer* timer);
int adjusted_wait(Timer* timer);
int wait(uint64_t period_ms);
double get_diff_ms(Timer* timer);
void release_timer(Timer* timer);

#define TOGGLE_TIMER_RESOLUTION()
