/*
 * mode_standing.c - 11. Standing wave: a whole number n of wavelengths fits
 * around the ring, A(theta, t) = cos(n * theta) * cos(w * t). The 2n nodes
 * stay still while the antinodes between them swell and shrink in place
 * (two equal waves travelling in opposite directions). n steps up and down
 * through a range, changing only when the whole wave passes through zero,
 * so every change is seamless.
 */
#include "mode.h"
#include "fx.h"

/* ---- Tweakable ------------------------------------------------------- */
#define STANDING_N_MIN     2     /* fewest wavelengths around the ring */
#define STANDING_N_MAX     6     /* most */
#define STANDING_PERIOD_MS 1800  /* one full oscillation (cos(wt) through 2 pi) */
#define STANDING_HOLD      3     /* full oscillations before changing n */
#define STANDING_PEAK      255
/* 0: brightness = |A|          - every antinode pulses, nodes dark.
 * 1: brightness = (1 + A) / 2  - neighbouring lobes swap, nodes half lit. */
#ifndef STANDING_SIGNED
#define STANDING_SIGNED    1
#endif
/* ---------------------------------------------------------------------- */

static uint32_t t_ms;   /* time within the current n */
static uint8_t n;
static int8_t dir;      /* +1 counting up, -1 down */

static void init(uint32_t seed)
{
    (void)seed;
    t_ms = 0;
    n = STANDING_N_MIN;
    dir = 1;
}

static void update(uint32_t frame_ms, uint8_t out[RING_LEDS])
{
    t_ms += frame_ms;
    /* Change n a quarter period past the end of the hold: cos(wt) = 0 there,
     * so the ring is momentarily flat and the switch is invisible. */
    const uint32_t switch_at = STANDING_HOLD * STANDING_PERIOD_MS + STANDING_PERIOD_MS / 4;
    if (t_ms >= switch_at) {
        t_ms -= STANDING_HOLD * STANDING_PERIOD_MS; /* keep the same phase */
        if (n == STANDING_N_MAX) dir = -1;
        if (n == STANDING_N_MIN) dir = 1;
        n = (uint8_t)(n + dir);
    }

    uint16_t wt = (uint16_t)(((uint64_t)(t_ms % STANDING_PERIOD_MS) << 16) / STANDING_PERIOD_MS);
    int32_t time_amp = cos16(wt); /* Q15 */

    for (int i = 0; i < RING_LEDS; i++) {
        uint16_t th = ring_to_angle(ring_led_pos(i));
        int32_t a = ((int32_t)cos16((uint16_t)(th * n)) * time_amp) >> 15; /* Q15, -1..1 */
#if STANDING_SIGNED
        uint32_t v = (uint32_t)(a + 32768) >> 8;          /* (1 + A) / 2 */
#else
        uint32_t v = (uint32_t)(a < 0 ? -a : a) >> 7;     /* |A| */
#endif
        if (v > 255U) v = 255U;
        out[i] = (uint8_t)(v * STANDING_PEAK / 255U);
    }
}

const mode_def_t mode_standing = {"standing wave", init, update};
