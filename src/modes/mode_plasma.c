/*
 * mode_plasma.c - 9. Plasma: three sine waves with different wavelengths
 * and speeds added together around the ring, giving slowly drifting
 * interference patterns. Wavelengths are whole cycles per ring, so the
 * pattern is seamless at D40/D1.
 */
#include "mode.h"
#include "fx.h"

/* ---- Tweakable ------------------------------------------------------- */
/* Cycles per ring (integers), drift period in ms (negative = other way),
 * and relative strength (0..256) of each wave. */
#define P1_CYCLES 2
#define P1_MS     9000
#define P1_AMP    256
#define P2_CYCLES 3
#define P2_MS     (-13000)
#define P2_AMP    200
#define P3_CYCLES 5
#define P3_MS     7000
#define P3_AMP    150
#define PLASMA_PEAK 230   /* brightness of the brightest spots */
/* ---------------------------------------------------------------------- */

typedef struct {
    uint8_t cycles;
    int32_t period_ms;
    uint16_t amp;
} wave_t;

static const wave_t WAVES[3] = {
    {P1_CYCLES, P1_MS, P1_AMP},
    {P2_CYCLES, P2_MS, P2_AMP},
    {P3_CYCLES, P3_MS, P3_AMP},
};
#define AMP_SUM (P1_AMP + P2_AMP + P3_AMP)

static uint16_t phase[3]; /* binary angle */
static uint32_t frac[3];  /* sub-step phase remainder */

static void init(uint32_t seed)
{
    (void)seed;
    for (int k = 0; k < 3; k++) {
        phase[k] = (uint16_t)(k * 21845U); /* spread the starting phases */
        frac[k] = 0;
    }
}

static void update(uint32_t frame_ms, uint8_t out[RING_LEDS])
{
    for (int k = 0; k < 3; k++) {
        int32_t p = WAVES[k].period_ms;
        uint32_t per = (uint32_t)(p < 0 ? -p : p);
        /* phase step = 65536 * dt / period, keeping the remainder */
        frac[k] += 65536U * frame_ms;
        uint16_t step = (uint16_t)(frac[k] / per);
        frac[k] %= per;
        phase[k] = (uint16_t)(p < 0 ? phase[k] - step : phase[k] + step);
    }

    for (int i = 0; i < RING_LEDS; i++) {
        uint16_t th = ring_to_angle(ring_led_pos(i));
        int32_t sum = 0;
        for (int k = 0; k < 3; k++) {
            uint16_t a = (uint16_t)(th * WAVES[k].cycles + phase[k]);
            sum += (int32_t)sin16(a) * WAVES[k].amp;
        }
        /* sum in -AMP_SUM..AMP_SUM (Q15) -> u in 0..255 */
        int32_t u = (sum / AMP_SUM + 32768) >> 8;
        if (u < 0) u = 0;
        if (u > 255) u = 255;
        /* Contrast: blend of u and u^2 deepens the dark gaps. */
        uint32_t c = ((uint32_t)u + (uint32_t)(u * u) / 255U) / 2U;
        out[i] = (uint8_t)(c * PLASMA_PEAK / 255U);
    }
}

const mode_def_t mode_plasma = {"plasma", init, update};
