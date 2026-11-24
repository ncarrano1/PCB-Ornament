/*
 * fx.h - fixed-point helpers and drawing primitives for modes. Pure C.
 * No floating point anywhere (the Cortex-M3 has no FPU).
 */
#ifndef FX_H
#define FX_H

#include <stdint.h>
#include "ring_geom.h"

/* ---- Math ---------------------------------------------------------- */

/* Sine/cosine of a binary angle (65536 = one turn), Q15. */
int16_t sin16(uint16_t angle);
static inline int16_t cos16(uint16_t angle) { return sin16((uint16_t)(angle + 16384U)); }

/* Ring units -> binary angle. */
static inline uint16_t ring_to_angle(uint32_t pos)
{
    return (uint16_t)((pos % RING_POS) * 32U / 5U); /* 65536 / 10240 = 6.4 */
}

/* exp(-x / 256) * 65535 for x >= 0 (x in 1/256 e-folds). 0 beyond ~8. */
uint16_t exp_neg(uint32_t x_q8);

/* Integer square root. */
uint32_t isqrt32(uint32_t x);

/* a * b / 255, rounded, for 0..255 values. */
static inline uint8_t scale8(uint8_t a, uint8_t b)
{
    return (uint8_t)(((uint32_t)a * b + 127U) / 255U);
}

/* Smoothstep on 0..65535 -> 0..65535. */
uint16_t smooth16(uint16_t t);

/* ---- Random numbers (xorshift32) ------------------------------------ */

static inline uint32_t xs32(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

/* Uniform integer in lo..hi inclusive. */
static inline uint32_t xs_range(uint32_t *s, uint32_t lo, uint32_t hi)
{
    return lo + (uint32_t)(((uint64_t)xs32(s) * (hi - lo + 1U)) >> 32);
}

/* Never let a seed be 0 (xorshift would stick at 0). */
static inline uint32_t xs_seed(uint32_t seed) { return seed ? seed : 0x9E3779B9U; }

/* ---- Drawing -------------------------------------------------------- */
/*
 * Modes draw into a uint16_t accumulator of perceptual brightness and
 * convert with fx_out(). Adds saturate at 255 when converted.
 */
typedef uint16_t fx_buf_t[RING_LEDS];

void fx_clear(fx_buf_t buf);

/* Add v (0..255) at LED i, saturating. */
void fx_add(fx_buf_t buf, int i, uint32_t v);

/* Raise LED i to at least v. */
void fx_max(fx_buf_t buf, int i, uint32_t v);

/*
 * A point of light at a sub-LED position, split between the two nearest
 * LEDs so the total light stays constant as it moves (no dip in between).
 */
void fx_point(fx_buf_t buf, uint32_t pos, uint8_t v);

/* A raised-cosine blob of half-width `radius` (ring units), peak v. */
void fx_blob(fx_buf_t buf, uint32_t pos, uint32_t radius, uint8_t v);

/*
 * A comet: head at `head` moving clockwise (dir > 0) or counter-clockwise
 * (dir < 0), with a tail fading as exp(-distance / tail_leds), cut off
 * smoothly at tail_max LEDs. Combined with what's in buf using max.
 */
void fx_comet(fx_buf_t buf, uint32_t head, int dir, uint32_t tail_leds,
              uint32_t tail_max, uint8_t peak);

/* Clamp to 0..255 into the output buffer. */
void fx_out(const fx_buf_t buf, uint8_t out[RING_LEDS]);

#endif
