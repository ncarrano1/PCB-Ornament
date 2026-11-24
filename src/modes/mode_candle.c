/*
 * mode_candle.c - 8. Candle: the whole ring flickers like a flame.
 * Smooth random flicker in overall brightness, an occasional gust, small
 * per-LED variation, brighter at the bottom and dimmer at the top.
 */
#include "mode.h"
#include "fx.h"

/* ---- Tweakable ------------------------------------------------------- */
#define CANDLE_LEVEL      210   /* overall brightness, 0..255 */
#define CANDLE_FLOOR_PCT  55    /* slow flicker swings between this and 100 % */
#define CANDLE_SLOW_MIN   90    /* slow flicker: ms per random target */
#define CANDLE_SLOW_MAX   260
#define CANDLE_FAST_PCT   12    /* fast jitter depth, % */
#define CANDLE_FAST_MIN   30
#define CANDLE_FAST_MAX   70
#define CANDLE_LED_PCT    10    /* per-LED variation, % */
#define CANDLE_LED_MIN    120
#define CANDLE_LED_MAX    320
#define CANDLE_TOP_PCT    35    /* brightness at the top relative to the bottom */
#define CANDLE_GUST_MIN   4000  /* time between gusts */
#define CANDLE_GUST_MAX   11000
#define CANDLE_GUST_DIP   45    /* brightness at the bottom of a gust, % */
#define CANDLE_GUST_DOWN  90    /* ms to dip */
#define CANDLE_GUST_UP    600   /* ms to recover */
/* ---------------------------------------------------------------------- */

/* Value noise: glide smoothly from one random target to the next. */
typedef struct {
    uint16_t from, to;  /* 0..65535 */
    uint16_t t, dur;    /* ms */
} noise_t;

static noise_t slow, fast, led[RING_LEDS];
static uint32_t rng;
static int32_t gust_wait;
static uint32_t gust_t;     /* ms into a gust, 0 = none */
static uint8_t grad[RING_LEDS];

static void noise_next(noise_t *n, uint32_t dmin, uint32_t dmax)
{
    n->from = n->to;
    n->to = (uint16_t)xs32(&rng);
    n->t = 0;
    n->dur = (uint16_t)xs_range(&rng, dmin, dmax);
}

static uint16_t noise_step(noise_t *n, uint32_t dt, uint32_t dmin, uint32_t dmax)
{
    n->t = (uint16_t)(n->t + dt);
    while (n->t >= n->dur) {
        uint16_t over = (uint16_t)(n->t - n->dur);
        noise_next(n, dmin, dmax);
        n->t = over;
    }
    uint32_t a = smooth16((uint16_t)(((uint32_t)n->t * 65535U) / n->dur));
    return (uint16_t)(n->from + (((int32_t)n->to - n->from) * (int32_t)(a >> 1)) / 32768);
}

static void init(uint32_t seed)
{
    rng = xs_seed(seed);
    slow.to = 50000;
    fast.to = 32768;
    noise_next(&slow, CANDLE_SLOW_MIN, CANDLE_SLOW_MAX);
    noise_next(&fast, CANDLE_FAST_MIN, CANDLE_FAST_MAX);
    for (int i = 0; i < RING_LEDS; i++) {
        led[i].to = (uint16_t)xs32(&rng);
        noise_next(&led[i], CANDLE_LED_MIN, CANDLE_LED_MAX);
        led[i].t = (uint16_t)xs_range(&rng, 0, led[i].dur - 1U);
        /* Height gradient: 100 % at the bottom, CANDLE_TOP_PCT at the top. */
        uint32_t down = (uint32_t)(32767 - height(i)) / 2U; /* 0 top .. 32767 bottom */
        grad[i] = (uint8_t)(255U * (CANDLE_TOP_PCT * 32767U + (100U - CANDLE_TOP_PCT) * down)
                            / (100U * 32767U));
    }
    gust_wait = (int32_t)xs_range(&rng, CANDLE_GUST_MIN, CANDLE_GUST_MAX);
    gust_t = 0;
}

static void update(uint32_t frame_ms, uint8_t out[RING_LEDS])
{
    /* Slow flicker: FLOOR..100 %. Fast jitter: +-FAST/2 %. Q16. */
    uint32_t s = noise_step(&slow, frame_ms, CANDLE_SLOW_MIN, CANDLE_SLOW_MAX);
    uint32_t level = (CANDLE_FLOOR_PCT * 65535U + (100U - CANDLE_FLOOR_PCT) * s) / 100U;
    uint32_t f = noise_step(&fast, frame_ms, CANDLE_FAST_MIN, CANDLE_FAST_MAX);
    uint32_t jitter = 65536U - CANDLE_FAST_PCT * 65536U / 200U + (CANDLE_FAST_PCT * f) / 100U;
    level = (uint32_t)(((uint64_t)level * jitter) >> 16); /* can exceed 32 bits */
    if (level > 65535U) level = 65535U;

    /* Gust: quick dip, slow recovery. */
    if (gust_t == 0) {
        gust_wait -= (int32_t)frame_ms;
        if (gust_wait <= 0) gust_t = 1;
    }
    if (gust_t) {
        uint32_t depth; /* 0 = no dip, 65535 = full dip */
        if (gust_t < CANDLE_GUST_DOWN) {
            depth = smooth16((uint16_t)(gust_t * 65535U / CANDLE_GUST_DOWN));
        } else if (gust_t < CANDLE_GUST_DOWN + CANDLE_GUST_UP) {
            depth = 65535U - smooth16((uint16_t)((gust_t - CANDLE_GUST_DOWN) * 65535U / CANDLE_GUST_UP));
        } else {
            depth = 0;
            gust_t = 0;
            gust_wait = (int32_t)xs_range(&rng, CANDLE_GUST_MIN, CANDLE_GUST_MAX);
        }
        if (gust_t) gust_t += frame_ms;
        level = level * (65536U - depth * (100U - CANDLE_GUST_DIP) / 100U) >> 16;
    }

    for (int i = 0; i < RING_LEDS; i++) {
        uint32_t n = noise_step(&led[i], frame_ms, CANDLE_LED_MIN, CANDLE_LED_MAX);
        uint32_t var = 65536U - CANDLE_LED_PCT * 65536U / 200U + (CANDLE_LED_PCT * n) / 100U;
        uint32_t v = (CANDLE_LEVEL * level) >> 16;
        v = (v * var) >> 16;
        v = (v * grad[i]) / 255U;
        out[i] = (uint8_t)(v > 255U ? 255U : v);
    }
}

const mode_def_t mode_candle = {"candle", init, update};
