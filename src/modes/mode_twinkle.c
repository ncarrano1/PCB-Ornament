/*
 * mode_twinkle.c - 4. Twinkle: fairy lights. Random LEDs fade in and out
 * at random rates and peaks, never in sync, with a few always lit.
 */
#include "mode.h"
#include "fx.h"

/* ---- Tweakable ------------------------------------------------------- */
#define TW_LIFE_MIN_MS 600    /* fade in + out time, shortest */
#define TW_LIFE_MAX_MS 2500   /* ... longest */
#define TW_IDLE_MIN_MS 400    /* dark gap between twinkles, shortest */
#define TW_IDLE_MAX_MS 5000   /* ... longest */
#define TW_PEAK_MIN    80     /* dimmest twinkle peak, 0..255 */
#define TW_PEAK_MAX    255
#define TW_MIN_LIT     4      /* always at least this many twinkling */
#define TW_START_PCT   35     /* share of LEDs already mid-twinkle at start */
/* ---------------------------------------------------------------------- */

typedef struct {
    uint16_t life;  /* total twinkle time, 0 = idle */
    uint16_t age;   /* ms into the twinkle */
    int32_t idle;   /* ms until the next twinkle when idle */
    uint8_t peak;
} star_t;

static star_t stars[RING_LEDS];
static uint32_t rng;

static void start(star_t *s)
{
    s->life = (uint16_t)xs_range(&rng, TW_LIFE_MIN_MS, TW_LIFE_MAX_MS);
    s->age = 0;
    s->peak = (uint8_t)xs_range(&rng, TW_PEAK_MIN, TW_PEAK_MAX);
}

static void init(uint32_t seed)
{
    rng = xs_seed(seed);
    for (int i = 0; i < RING_LEDS; i++) {
        star_t *s = &stars[i];
        if (xs_range(&rng, 0, 99) < TW_START_PCT) {
            start(s);
            s->age = (uint16_t)xs_range(&rng, 0, s->life - 1U);
        } else {
            s->life = 0;
            s->idle = (int32_t)xs_range(&rng, 0, TW_IDLE_MAX_MS);
        }
    }
}

static void update(uint32_t frame_ms, uint8_t out[RING_LEDS])
{
    int active = 0;
    for (int i = 0; i < RING_LEDS; i++) {
        star_t *s = &stars[i];
        if (s->life) {
            s->age = (uint16_t)(s->age + frame_ms);
            if (s->age >= s->life) {
                s->life = 0;
                s->idle = (int32_t)xs_range(&rng, TW_IDLE_MIN_MS, TW_IDLE_MAX_MS);
            }
        } else {
            s->idle -= (int32_t)frame_ms;
            if (s->idle <= 0) {
                start(s);
            }
        }
        active += s->life != 0;
    }

    /* Keep a few lit: wake random idle LEDs early. */
    while (active < TW_MIN_LIT) {
        int i = (int)xs_range(&rng, 0, RING_LEDS - 1);
        while (stars[i].life) {
            i = (i + 1) % RING_LEDS;
        }
        start(&stars[i]);
        active++;
    }

    for (int i = 0; i < RING_LEDS; i++) {
        const star_t *s = &stars[i];
        uint32_t v = 0;
        if (s->life) {
            /* half-sine envelope over the twinkle's life */
            uint16_t ang = (uint16_t)(((uint32_t)s->age * 32768U) / s->life);
            int32_t sn = sin16(ang);
            v = ((uint32_t)s->peak * (uint32_t)(sn < 0 ? 0 : sn)) >> 15;
        }
        out[i] = (uint8_t)v;
    }
}

const mode_def_t mode_twinkle = {"twinkle", init, update};
