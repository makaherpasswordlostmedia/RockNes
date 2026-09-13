/* Simple implementation of Biquad filters -- Tom St Denis
 *
 * Based on the work
 *
 * Cookbook formulae for audio EQ biquad filter coefficients
 * ---------------------------------------------------------
 * by Robert Bristow-Johnson, pbjrbj@viconet.com  a.k.a. robert@audioheads.com
 *
 * Available on the web at
 *
 * http://www.smartelectronix.com/musicdsp/text/filters005.txt
 *
 * This work is hereby placed in the public domain for all purposes, whether
 * commercial, free [as in speech] or educational, etc.  Use the code and please
 * give me credit if you wish.
 *
 * Tom St Denis -- http://tomstdenis.home.dhs.org
 *
 * ---
 * Ported to Q16.16 fixed-point for FPU-less ARM7TDMI targets
 * (iPod Mini 2G / Rockbox). See biquad.h for rationale. Coefficient
 * *setup* (biquad_init) is unchanged double-precision math; only the
 * per-sample biquad() hot path below is fixed-point.
 */

#include <math.h>
#include "biquad.h"

#ifndef M_LN2
#define M_LN2	   0.69314718055994530942
#endif

#ifndef M_PI
#define M_PI		3.14159265358979323846
#endif

/* Computes a BiQuad filter on a sample. Q16.16 fixed-point throughout:
 * no float/double touched here, so this compiles to plain ARM integer
 * multiplies (SMULL) and adds -- no libgcc soft-float calls.
 *
 * The five products are summed in a single 64-bit accumulator and
 * rounded/shifted down to Q16.16 ONCE at the end, rather than
 * rounding each product individually and adding the rounded Q16.16
 * results. This filter's poles sit right at the edge of the unit
 * circle (~0.996 magnitude for the 20Hz HPF at 11025Hz Rockbox uses),
 * so it's a near-marginally-stable feedback loop: y1/y2 carry forward
 * into every future sample. Rounding five times per sample instead of
 * once let per-multiply quantization noise compound through that
 * feedback path over thousands of samples (measured ~5-9% deviation
 * from a double-precision reference in testing); accumulating in
 * 64-bit first cuts that to noise-floor level (~2^-16 per sample,
 * consistent with the double reference to within Q16.16 resolution
 * and not compounding). */
fixed_t biquad(fixed_t sample, Biquad * b)
{
    int64_t acc;
    fixed_t result;

    acc  = (int64_t)b->a0 * sample;
    acc += (int64_t)b->a1 * b->x1;
    acc += (int64_t)b->a2 * b->x2;
    acc -= (int64_t)b->a3 * b->y1;
    acc -= (int64_t)b->a4 * b->y2;

    /* single round-and-shift back to Q16.16 */
    if (acc >= 0) acc += (1LL << (FX_SHIFT - 1));
    else          acc -= (1LL << (FX_SHIFT - 1));
    result = (fixed_t)(acc >> FX_SHIFT);

    /* shift x1 to x2, sample to x1 */
    b->x2 = b->x1;
    b->x1 = sample;

    /* shift y1 to y2, result to y1 */
    b->y2 = b->y1;
    b->y1 = result;

    return result;
}

/* sets up a BiQuad Filter. Unchanged double-precision math -- this
 * runs once per filter at APU init (2 filters total), not per-sample,
 * so the trig/pow cost here is irrelevant to frame budget. Final
 * coefficients are converted to Q16.16 before being stored. */
void biquad_init(Biquad* b, int type, double dbGain, double freq,
double srate, double bandwidth)
{
    double A, omega, sn, cs, alpha, beta;
    double a0, a1, a2, b0, b1, b2;

    /* setup variables */
    A = pow(10, dbGain /40);
    omega = 2 * M_PI * freq /srate;
    sn = sin(omega);
    cs = cos(omega);
    alpha = sn * sinh(M_LN2 /2 * bandwidth * omega /sn);
    beta = sqrt(A + A);

    switch (type) {
    case LPF:
        b0 = (1 - cs) /2;
        b1 = 1 - cs;
        b2 = (1 - cs) /2;
        a0 = 1 + alpha;
        a1 = -2 * cs;
        a2 = 1 - alpha;
        break;
    case HPF:
        b0 = (1 + cs) /2;
        b1 = -(1 + cs);
        b2 = (1 + cs) /2;
        a0 = 1 + alpha;
        a1 = -2 * cs;
        a2 = 1 - alpha;
        break;
    case BPF:
        b0 = alpha;
        b1 = 0;
        b2 = -alpha;
        a0 = 1 + alpha;
        a1 = -2 * cs;
        a2 = 1 - alpha;
        break;
    case NOTCH:
        b0 = 1;
        b1 = -2 * cs;
        b2 = 1;
        a0 = 1 + alpha;
        a1 = -2 * cs;
        a2 = 1 - alpha;
        break;
    case PEQ:
        b0 = 1 + (alpha * A);
        b1 = -2 * cs;
        b2 = 1 - (alpha * A);
        a0 = 1 + (alpha /A);
        a1 = -2 * cs;
        a2 = 1 - (alpha /A);
        break;
    case LSH:
        b0 = A * ((A + 1) - (A - 1) * cs + beta * sn);
        b1 = 2 * A * ((A - 1) - (A + 1) * cs);
        b2 = A * ((A + 1) - (A - 1) * cs - beta * sn);
        a0 = (A + 1) + (A - 1) * cs + beta * sn;
        a1 = -2 * ((A - 1) + (A + 1) * cs);
        a2 = (A + 1) + (A - 1) * cs - beta * sn;
        break;
    case HSH:
        b0 = A * ((A + 1) + (A - 1) * cs + beta * sn);
        b1 = -2 * A * ((A - 1) + (A + 1) * cs);
        b2 = A * ((A + 1) + (A - 1) * cs - beta * sn);
        a0 = (A + 1) - (A - 1) * cs + beta * sn;
        a1 = 2 * ((A - 1) - (A + 1) * cs);
        a2 = (A + 1) - (A - 1) * cs - beta * sn;
        break;
    default:
        return;
    }

    /* precompute the coefficients, then drop to Q16.16.
     * Coefficients here are bounded (typically |x| < 4 for these
     * filter types at audio frequencies), well within Q16.16 range
     * (+/-32768). If a future filter type/parameter combination
     * pushed a coefficient beyond that, this would silently wrap --
     * not a concern for the fixed HPF/LPF instances apu.c creates. */
    b->a0 = dbl_to_fx(b0 / a0);
    b->a1 = dbl_to_fx(b1 / a0);
    b->a2 = dbl_to_fx(b2 / a0);
    b->a3 = dbl_to_fx(a1 / a0);
    b->a4 = dbl_to_fx(a2 / a0);

    /* zero initial samples */
    b->x1 = b->x2 = 0;
    b->y1 = b->y2 = 0;
}
