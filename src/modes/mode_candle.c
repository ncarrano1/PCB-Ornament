/*
 * mode_candle.c - 8. Candle: the ring is a flame. It is always lit at the
 * bottom (the wick); the flame's height jumps around, it leans side to
 * side, its brightness flutters, and now and then a gust knocks it down.
 * Brighter at the bottom, dimmer and more restless at the top.
 */
#include "mode.h"
#include "fx.h"

/* ---- Tweakable ------------------------------------------------------- */
#define CANDLE_PEAK        235  /* brightness at the base of the flame */
#define CANDLE_FLOOR_PCT   22   /* ember glow left around the rest of the ring */
#define FLAME_H_MIN_DEG    55   /* flame reaches this far up from the bottom... */
#define FLAME_H_MAX_DEG    165  /* ...up to this far */
#define FLAME_H_MIN_MS     50   /* flame height: ms per random target */
#define FLAME_H_MAX_MS     160
#define FLAME_LEAN_DEG     28   /* max lean left/right */
#define FLAME_LEAN_MIN_MS  180
#define FLAME_LEAN_MAX_MS  480
#define FLICKER_MIN_PCT    40   /* brightness flutters between this and 100 % */
#define FLICKER_MIN_MS     35
#define FLICKER_MAX_MS     110
#define CANDLE_LED_PCT     12   /* per-LED shimmer, % */
#define CANDLE_LED_MIN_MS  90
#define CANDLE_LED_MAX_MS  260
#define GUST_MIN_MS        3000 /* time between gusts */
#define GUST_MAX_MS        8000
#define GUST_DOWN_MS       70   /* flame knocked down... */
#define GUST_UP_MS         550  /* ...and recovering */
#define GUST_HEIGHT_DEG    35   /* flame height at the bottom of a gust */
#define GUST_DIM_PCT       45   /* brightness at the bottom of a gust */
/* ---------------------------------------------------------------------- */

#define DEG(d) ((int32_t)RING_POS * (d) / 360)

/* Value noise: glide smoothly from one random target (0..65535) to the next. */
typedef struct {
    uint16_t from, to;
    uint16_t t, dur;
} noise_t;

static noise_t n_height, n_lean, n_flicker, n_led[RING_LEDS];
static uint32_t rng;
static int32_t gust_wait;
static uint32_t gust_t; /* ms into a gust, 0 = none */

static void noise_next(noise_t *n, uint32_t dmin, uint32_t dmax)
{
    n->from = n->to;
    n->to = (uint16_t)xs32(&rng);
    n->t = 0;
    n->dur = (uint16_t)xs_range(&rng, dmin, dmax);
}

static void noise_init(noise_t *n, uint32_t dmin, uint32_t dmax)
{
    n->to = (uint16_t)xs32(&rng);
    noise_next(n, dmin, dmax);
    n->t = (uint16_t)xs_range(&rng, 0, n->dur - 1U);
}

static uint32_t noise_step(noise_t *n, uint32_t dt, uint32_t dmin, uint32_t dmax)
{
    n->t = (uint16_t)(n->t + dt);
    while (n->t >= n->dur) {
        uint16_t over = (uint16_t)(n->t - n->dur);
        noise_next(n, dmin, dmax);
        n->t = over;
    }
    int32_t a = smooth16((uint16_t)(((uint32_t)n->t * 65535U) / n->dur)) >> 1; /* Q15 */
    return (uint32_t)(n->from + (((int32_t)n->to - n->from) * a) / 32768);
}

static void init(uint32_t seed)
{
    rng = xs_seed(seed);
    noise_init(&n_height, FLAME_H_MIN_MS, FLAME_H_MAX_MS);
    noise_init(&n_lean, FLAME_LEAN_MIN_MS, FLAME_LEAN_MAX_MS);
    noise_init(&n_flicker, FLICKER_MIN_MS, FLICKER_MAX_MS);
    for (int i = 0; i < RING_LEDS; i++) {
        noise_init(&n_led[i], CANDLE_LED_MIN_MS, CANDLE_LED_MAX_MS);
    }
    gust_wait = (int32_t)xs_range(&rng, GUST_MIN_MS / 2, GUST_MAX_MS);
    gust_t = 0;
}

/* Gust strength 0..65535: quick knock-down, slower recovery. */
static uint32_t gust_step(uint32_t dt)
{
    if (gust_t == 0) {
        gust_wait -= (int32_t)dt;
        if (gust_wait > 0) return 0;
        gust_t = 1;
    }
    uint32_t g;
    if (gust_t < GUST_DOWN_MS) {
        g = smooth16((uint16_t)(gust_t * 65535U / GUST_DOWN_MS));
    } else if (gust_t < GUST_DOWN_MS + GUST_UP_MS) {
        g = 65535U - smooth16((uint16_t)((gust_t - GUST_DOWN_MS) * 65535U / GUST_UP_MS));
    } else {
        gust_t = 0;
        gust_wait = (int32_t)xs_range(&rng, GUST_MIN_MS, GUST_MAX_MS);
        return 0;
    }
    gust_t += dt;
    return g;
}

static void update(uint32_t frame_ms, uint8_t out[RING_LEDS])
{
    uint32_t g = gust_step(frame_ms);

    /* Flame height (ring units from the bottom), pulled down by gusts. */
    uint32_t hn = noise_step(&n_height, frame_ms, FLAME_H_MIN_MS, FLAME_H_MAX_MS);
    int32_t h = DEG(FLAME_H_MIN_DEG) + (int32_t)(((int64_t)(DEG(FLAME_H_MAX_DEG) - DEG(FLAME_H_MIN_DEG)) * hn) >> 16);
    h -= (int32_t)(((int64_t)(h - DEG(GUST_HEIGHT_DEG)) * g) >> 16);

    /* Lean: shifts the flame toward one side. */
    int32_t ln = (int32_t)noise_step(&n_lean, frame_ms, FLAME_LEAN_MIN_MS, FLAME_LEAN_MAX_MS) - 32768;
    int32_t lean = (DEG(FLAME_LEAN_DEG) * ln) / 32768;

    /* Brightness flutter, dimmed by gusts. Q16. */
    uint32_t fn = noise_step(&n_flicker, frame_ms, FLICKER_MIN_MS, FLICKER_MAX_MS);
    uint32_t bright = (FLICKER_MIN_PCT * 65535U + (100U - FLICKER_MIN_PCT) * fn) / 100U;
    bright = (uint32_t)(((uint64_t)bright * (65536U - g * (100U - GUST_DIM_PCT) / 100U)) >> 16);

    for (int i = 0; i < RING_LEDS; i++) {
        /* Signed distance from the bottom along the ring: <0 right side, >0 left. */
        int32_t d = (int32_t)ring_led_pos(i) - RING_POS_BOTTOM - lean;
        uint32_t ad = (uint32_t)(d < 0 ? -d : d);

        /* Flame profile: full at the base, smooth falloff to the tip at h. */
        uint32_t core = 0;
        if (ad < (uint32_t)h) {
            core = 65535U - smooth16((uint16_t)(ad * 65535U / (uint32_t)h));
        }
        uint32_t lvl = CANDLE_FLOOR_PCT * 65535U / 100U +
                       (uint32_t)(((uint64_t)(100U - CANDLE_FLOOR_PCT) * core) / 100U);

        uint32_t sh = noise_step(&n_led[i], frame_ms, CANDLE_LED_MIN_MS, CANDLE_LED_MAX_MS);
        uint32_t shimmer = 65536U - CANDLE_LED_PCT * 65536U / 200U + (CANDLE_LED_PCT * sh) / 100U;

        uint64_t v = (uint64_t)CANDLE_PEAK * lvl;
        v = (v * bright) >> 16;
        v = (v * shimmer) >> 16;
        v >>= 16;
        out[i] = (uint8_t)(v > 255U ? 255U : v);
    }
}

const mode_def_t mode_candle = {"candle", init, update};
