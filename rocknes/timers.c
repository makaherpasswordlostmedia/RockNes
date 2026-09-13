#include "timers.h"
#include "plugin.h"

extern const struct plugin_api* rb;

#define NS_PER_TICK (10L * 1000L * 1000L) /* 10ms in ns, HZ=100 */

void init_timer(Timer* timer, uint64_t period_ns){
    timer->period_ticks = (long)(period_ns / NS_PER_TICK);
    if (timer->period_ticks < 1) timer->period_ticks = 1;
    timer->start_tick = 0;
    timer->last_diff_ticks = 0;
}

void mark_start(Timer* timer){
    timer->start_tick = *rb->current_tick;
}

void mark_end(Timer* timer){
    timer->last_diff_ticks = *rb->current_tick - timer->start_tick;
}

int adjusted_wait(Timer* timer){
    long remaining = timer->period_ticks - timer->last_diff_ticks;
    if (remaining > 0) {
        rb->sleep(remaining);
    }
    return 0;
}

int wait(uint64_t period_ms){
    /* round up so short waits still yield at least one tick */
    long ticks = (long)((period_ms + 9) / 10);
    if (ticks < 1) ticks = 1;
    rb->sleep(ticks);
    return 0;
}

double get_diff_ms(Timer* timer){
    return (double)timer->last_diff_ticks * 10.0;
}

void release_timer(Timer* timer){
    (void)timer; /* nothing to free - no heap allocation on this backend */
}
