/*
 * mode_plasma.c - 9. Plasma: sine waves of different wavelengths and
 * speeds are added together around the ring, the ring itself is gently
 * warped, and the sum is shown through a cycling sine "palette". The result
 * is a few crisp bright bands that stream, merge, split and change
 * direction. Every wave has a whole number of cycles per ring, so the
 * pattern is seamless at D40/D1.
 */
#include "mode.h"
#include "fx.h"

/* ---- Tweakable ------------------------------------------------------- */
/* Waves: cycles per ring (integer), base drift period in ms (negative =
 * counter-clockwise), strength 0..256. */
#define W1_CYCLES 2
#define W1_MS     6000
#define W1_AMP    256
#define W2_CYCLES 3
#define W2_MS     (-8500)
#define W2_AMP    200
#define W3_CYCLES 1
#define W3_MS     11000
#define W3_AMP    170
#define SPEED_SWING_PCT 70     /* each wave's speed wanders +-this % ... */
#define SPEED_SWING_MS  17000  /* ...over this period */
#define WARP_DEG        50     /* how far the ring is stretched/squeezed */
#define WARP_MS         9000   /* warp drift period */
#define PALETTE_BANDS   3      /* bands per unit of wave sum (pattern density) */
#define PALETTE_MS      2600   /* palette cycling period: band motion speed */
#define PLASMA_PEAK     245
/* ---------------------------------------------------------------------- */

typedef struct {
    uint8_t cycles;
    int32_t period_ms;
    uint16_t amp;
} wave_t;

static const wave_t WAVES[3] = {
    {W1_CYCLES, W1_MS, W1_AMP},
    {W2_CYCLES, W2_MS, W2_AMP},
    {W3_CYCLES, W3_MS, W3_AMP},
};
#define AMP_SUM (W1_AMP + W2_AMP + W3_AMP)

/* Phases as 32-bit binary angles (2^32 = one turn) for smooth slow motion. */
static uint32_t ph[3], ph_warp, ph_pal;
static uint32_t t_ms;

static void init(uint32_t seed)
{
    (void)seed;
    ph[0] = 0x00000000U;
    ph[1] = 0x55555555U;
    ph[2] = 0xAAAAAAAAU;
    ph_warp = 0x40000000U;
    ph_pal = 0;
    t_ms = 0;
}

/* Phase step for `ms` at one turn per `period` ms, scaled by speed (Q16). */
static uint32_t step(int32_t period, uint32_t ms, uint32_t speed_q16)
{
    uint32_t per = (uint32_t)(period < 0 ? -period : period);
    uint64_t d = (((uint64_t)ms << 32) / per * speed_q16) >> 16;
    return period < 0 ? (uint32_t)(0U - (uint32_t)d) : (uint32_t)d;
}

static void update(uint32_t frame_ms, uint8_t out[RING_LEDS])
{
    t_ms += frame_ms;
    for (int k = 0; k < 3; k++) {
        /* Speed wanders: 1 +- SWING * sin(t / SWING_MS + offset). */
        uint16_t a = (uint16_t)(((uint64_t)(t_ms % SPEED_SWING_MS) << 16) / SPEED_SWING_MS
                                + (uint32_t)k * 21845U);
        int32_t sw = (SPEED_SWING_PCT * (int32_t)sin16(a)) / 100;   /* Q15 */
        uint32_t speed = (uint32_t)(65536 + sw * 2);                 /* Q16 */
        ph[k] += step(WAVES[k].period_ms, frame_ms, speed);
    }
    ph_warp += step(WARP_MS, frame_ms, 65536U);
    ph_pal += step(PALETTE_MS, frame_ms, 65536U);

    const int32_t warp_amp = (int32_t)(65536L * WARP_DEG / 360);
    for (int i = 0; i < RING_LEDS; i++) {
        uint16_t th = ring_to_angle(ring_led_pos(i));
        /* Warp: theta' = theta + W sin(theta + phase) (periodic, so still seamless). */
        uint16_t tw = (uint16_t)(th + ((warp_amp * sin16((uint16_t)(th + (ph_warp >> 16)))) >> 15));

        int32_t sum = 0;
        for (int k = 0; k < 3; k++) {
            uint16_t a = (uint16_t)(tw * WAVES[k].cycles + (ph[k] >> 16));
            sum += (int32_t)sin16(a) * WAVES[k].amp;
        }
        sum /= AMP_SUM; /* -32767..32767 */

        /* Palette: brightness = ((1 + sin(BANDS * pi * sum + cycle)) / 2)^2 */
        uint16_t pa = (uint16_t)(((sum * PALETTE_BANDS) >> 1) + (int32_t)(ph_pal >> 16));
        uint32_t u = (uint32_t)(sin16(pa) + 32768) >> 8;   /* 0..255 */
        uint32_t c = u * u / 255U;
        out[i] = (uint8_t)(c * PLASMA_PEAK / 255U);
    }
}

const mode_def_t mode_plasma = {"plasma", init, update};
