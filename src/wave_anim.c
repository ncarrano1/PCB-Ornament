/*
 * wave_anim.c - traveling wave of brightness around the ring.
 * A raised-cosine bump with gamma correction moves clockwise
 * (D1 -> D2 -> ... -> D40 -> D1) and wraps seamlessly.
 */
#include "wave_anim.h"
#include <math.h>

/* ---- Easy-to-tweak constants ------------------------------------------ */
#define WAVE_WIDTH_LEDS   8      /* full width of a bump, in LEDs (2..20) */
#define SLOW_PERIOD_MS    4000   /* time for one turn of the ring, slow */
#define FAST_PERIOD_MS    1500   /* time for one turn of the ring, fast */
#define WAVE_PEAK_PCT     100    /* bump peak, % of the capped brightness */
#define WAVE_GAMMA        2.2f   /* perceptual correction */
/* ------------------------------------------------------------------------ */

#define LUT_SIZE  256
#define HALF_POS  (WAVE_WIDTH_LEDS * WAVE_POS_PER_LED / 2)

static uint16_t lut[LUT_SIZE + 1]; /* lut[0] = peak, lut[LUT_SIZE] = 0 */
static uint32_t phase;             /* 2^32 = one full turn */

void wave_anim_init(void)
{
    const float pi = 3.14159265f;
    const float peak = 65535.0f * WAVE_PEAK_PCT / 100.0f;

    for (int i = 0; i <= LUT_SIZE; i++) {
        float x = (float)i / LUT_SIZE;                 /* 0 = centre, 1 = edge */
        float shape = 0.5f * (1.0f + cosf(pi * x));    /* raised cosine */
        float v = powf(shape, WAVE_GAMMA) * peak;
        lut[i] = (uint16_t)(v + 0.5f);
    }
    lut[LUT_SIZE] = 0;
    phase = 0;
}

static uint32_t period_ms(wave_mode_t mode)
{
    return mode == WAVE_FAST ? FAST_PERIOD_MS : SLOW_PERIOD_MS;
}

void wave_anim_advance(wave_mode_t mode, uint32_t dt_ms)
{
    if (mode == WAVE_OFF) {
        return;
    }
    /* 2^32 / period per ms; phase wraps naturally at a full turn. */
    uint32_t per_ms = (uint32_t)(4294967296ULL / period_ms(mode));
    phase += per_ms * dt_ms;
}

/* Bump value at circular distance d (0..WAVE_RING_POS/2) from its centre. */
static uint16_t bump(uint32_t d)
{
    if (d >= HALF_POS) {
        return 0;
    }
    uint32_t idx = d * LUT_SIZE * 256 / HALF_POS; /* 8.8 fixed point */
    uint32_t i = idx >> 8, f = idx & 255;
    int32_t a = lut[i], b = lut[i + 1];
    return (uint16_t)(a + ((b - a) * (int32_t)f) / 256);
}

static uint32_t ring_dist(uint32_t a, uint32_t b)
{
    uint32_t d = a > b ? a - b : b - a;
    return d > WAVE_RING_POS / 2 ? WAVE_RING_POS - d : d;
}

void wave_anim_render_at(wave_mode_t mode, uint32_t pos,
                         uint16_t duty[BCM_LED_COUNT])
{
    pos %= WAVE_RING_POS;
    for (uint32_t i = 0; i < BCM_LED_COUNT; i++) {
        uint32_t led_pos = i * WAVE_POS_PER_LED;
        uint16_t v = 0;
        if (mode != WAVE_OFF) {
            v = bump(ring_dist(led_pos, pos));
        }
        if (mode == WAVE_TWO) {
            uint32_t opp = (pos + WAVE_RING_POS / 2) % WAVE_RING_POS;
            uint16_t v2 = bump(ring_dist(led_pos, opp));
            if (v2 > v) {
                v = v2;
            }
        }
        duty[i] = v;
    }
}

void wave_anim_render(wave_mode_t mode, uint16_t duty[BCM_LED_COUNT])
{
    uint32_t pos = (uint32_t)(((uint64_t)phase * WAVE_RING_POS) >> 32);
    wave_anim_render_at(mode, pos, duty);
}
