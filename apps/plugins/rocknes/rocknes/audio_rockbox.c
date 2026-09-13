/* audio_rockbox.c - replaces upstream's SDL_AudioStream push model with
 * Rockbox's pull-based mixer_channel_play_data callback, following the
 * same pattern Rockboy's rbsound.c uses.
 *
 * APU renders mono 16-bit PCM at SAMPLING_FREQUENCY (11025 Hz, see
 * apu.h) once per frame into a block; we copy that block into a ring
 * buffer here, and get_more() (called from IRQ/mixer context whenever
 * Rockbox needs more samples) drains it.
 */
#include "rocknes.h"
#include "plugin.h"

extern const struct plugin_api* rb;

/* ~0.25s of headroom at 11025 Hz mono - enough to absorb the frame-to-
 * frame jitter of an interpreted 6502+PPU running near its speed
 * budget, without adding so much latency input feels laggy. */
#define RING_SAMPLES 2816
#define RING_BYTES   (RING_SAMPLES * sizeof(int16_t))

static int16_t ring[RING_SAMPLES];
static volatile size_t write_pos = 0;
static volatile size_t read_pos  = 0;
static volatile size_t available = 0; /* samples ready to read */
static int started = 0;

/* mixer pull callback - runs in IRQ-ish context, must be fast and
 * must not block on anything that can itself block. */
static void get_more(const void** start, size_t* size)
{
    /* Rockbox wants a contiguous run; if the ring wraps, hand back
     * only the contiguous tail and let it call us again next time -
     * this mirrors how Rockboy's get_more deals with buffer wrap by
     * keeping BUF_SIZE a clean divisor, but since our fill rate is
     * variable (frame-driven) we instead just serve whatever
     * contiguous run is available, down to a small minimum, and
     * output silence if we underran. */
    static int16_t silence[64];

    size_t contiguous = RING_SAMPLES - read_pos;
    size_t avail = available;
    size_t serve = avail < contiguous ? avail : contiguous;

    if (serve == 0) {
        /* underrun - starvation means the emulator fell behind. Hand
         * back silence rather than replaying stale data (which would
         * sound like a stutter-loop). */
        for (size_t i = 0; i < 64; i++) silence[i] = 0;
        *start = silence;
        *size = sizeof(silence);
        return;
    }

    *start = &ring[read_pos];
    *size = serve * sizeof(int16_t);
    read_pos = (read_pos + serve) % RING_SAMPLES;
    available -= serve;
}

void rocknes_audio_init(void)
{
    write_pos = read_pos = available = 0;
    started = 0;

    rb->audio_stop();
#if INPUT_SRC_CAPS != 0
    rb->audio_set_input_source(AUDIO_SRC_PLAYBACK, SRCF_PLAYBACK);
    rb->audio_set_output_source(AUDIO_SRC_PLAYBACK);
#endif
    rb->mixer_set_frequency(SAMPR_11);
    rb->pcmbuf_fade(false, true);
}

void rocknes_audio_close(void)
{
    rb->pcmbuf_fade(false, false);
    rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
    rb->mixer_set_frequency(HW_SAMPR_DEFAULT);
}

void rocknes_audio_submit(const int16_t* samples, size_t count)
{
    if (count > RING_SAMPLES) count = RING_SAMPLES; /* clamp, shouldn't happen */

    for (size_t i = 0; i < count; i++) {
        ring[write_pos] = samples[i];
        write_pos = (write_pos + 1) % RING_SAMPLES;
    }
    /* if we overran (emulator produced faster than mixer drained),
     * drop the oldest data rather than growing without bound */
    if (available + count > RING_SAMPLES) {
        read_pos = write_pos; /* resync, accept the glitch */
        available = RING_SAMPLES;
    } else {
        available += count;
    }

    if (!started) {
        static const struct mixer_play_cbs cbs = {
            .get_more = get_more,
        };
        rb->mixer_channel_play_data(PCM_MIXER_CHAN_PLAYBACK, &cbs, NULL, 0);
        started = 1;
    }
}
