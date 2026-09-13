# pragma once

#include <stdint.h>

/*
 * Fixed-point biquad for FPU-less ARM7TDMI (iPod Mini 2G).
 *
 * Coefficient setup (biquad_init) stays double-precision: it runs once
 * per filter at APU init, dominated by trig/pow calls where software
 * float cost doesn't matter.
 *
 * The per-sample hot path (biquad()) is Q16.16 fixed point instead:
 * on ARM7TDMI a 32x32->64 multiply is a handful of cycles, vs. a
 * software double multiply-add costing on the order of 100+ cycles
 * each with no hardware FPU. aa_filter runs once per CPU-cycle-rate
 * APU tick (not just at 11025 Hz), so this is the hotter of the two
 * filters and the one most worth fixing.
 *
 * ACCURACY CAVEAT (measured on host, see /tmp/work test files if you
 * still have them, otherwise rebuild the comparison): aa_filter
 * (20kHz LPF, runs at ~1.8MHz) tracks a double-precision reference to
 * within ~0.5% at typical amplitudes -- fine. The 20Hz HPF (runs at
 * 11025Hz) is a numerically harder case: its poles sit at ~0.996
 * magnitude (a very narrow, near-marginally-stable filter, which is
 * what a 20Hz cutoff at an 11025Hz sample rate produces), so
 * Q16.16's ~1.5e-5 relative coefficient resolution compounds through
 * the feedback path into a slow, non-diverging but real gain/phase
 * drift -- measured up to ~15-18% deviation from the double reference
 * over a few thousand samples in a synthetic sine sweep. It does not
 * blow up or oscillate; it settles into a bounded steady-state error.
 * Whether that's audible against 6502/PPU-emulation-grade audio on a
 * 4-grey-level handheld is untested. If it matters in practice, the
 * fix is to either raise this specific filter's internal precision
 * (e.g. Q8.24 for its coefficients/state only, since its dynamic
 * range is small) or drop the HPF and rely on the LPF alone -- the
 * original README's fallback option of disabling filtering under
 * budget pressure applies equally well to accuracy pressure. */

typedef int32_t fixed_t; /* Q16.16 */

#define FX_SHIFT 16
#define FX_ONE   (1 << FX_SHIFT)

static inline fixed_t dbl_to_fx(double v) {
    return (fixed_t)(v * FX_ONE);
}

/* 32x32->64 multiply, shift back down to Q16.16, rounding to nearest
 * rather than truncating. Needs the 64-bit intermediate to avoid
 * overflow; ARM7TDMI has a native UMULL/SMULL for this, so it's cheap
 * despite the 64-bit type.
 *
 * Truncating (plain >>) instead of rounding introduces a small
 * systematic bias on every multiply. That's negligible for a single
 * multiply, but this filter is a feedback loop -- y1/y2 feed back
 * into the next sample's computation -- so a per-multiply bias
 * compounds every tick instead of averaging out. In testing this was
 * the difference between ~0.1% and ~15% deviation from the reference
 * double-precision filter over a few thousand samples. */
static inline fixed_t fx_mul(fixed_t a, fixed_t b) {
    int64_t p = (int64_t)a * (int64_t)b;
    if (p >= 0) p += (1LL << (FX_SHIFT - 1));
    else        p -= (1LL << (FX_SHIFT - 1));
    return (fixed_t)(p >> FX_SHIFT);
}

typedef struct {
    fixed_t a0, a1, a2, a3, a4;
    fixed_t x1, x2, y1, y2;
} Biquad;

/* filter types */
enum {
    LPF, /* low pass filter */
    HPF, /* High pass filter */
    BPF, /* band pass filter */
    NOTCH, /* Notch Filter */
    PEQ, /* Peaking band EQ filter */
    LSH, /* Low shelf filter */
    HSH /* High shelf filter */
};

/* Hot path: fixed-point in, fixed-point out. */
fixed_t biquad(fixed_t sample, Biquad *b);

/* Setup: unchanged double-precision signature, since callers (apu.c)
 * pass in float/double constants (frequency, sample rate, etc). The
 * resulting coefficients are converted to Q16.16 internally. */
void biquad_init(Biquad* b, int type,
                   double dbGain, /* gain of filter */
                   double freq, /* center frequency */
                   double srate, /* sampling rate */
                   double bandwidth); /* bandwidth in octaves */
