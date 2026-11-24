/*
 * mode_rain.c - 6. Raindrops: drops start at the top, fall down either
 * side under gravity (a bead on a hoop: speed grows with the height
 * dropped), and splash at the bottom as a quick flash that spreads one or
 * two LEDs sideways.
 */
#include "mode.h"
#include "fx.h"

/* ---- Tweakable ------------------------------------------------------- */
#define RAIN_GAP_MIN_MS   300   /* time between new drops */
#define RAIN_GAP_MAX_MS   900
#define RAIN_VMAX         9     /* speed at the bottom, ring units per ms (~35 LEDs/s) */
#define RAIN_START_Q8     230   /* drops start this far below the top (1/256 LED) */
#define RAIN_PEAK         220
#define RAIN_TAIL_MS      28    /* faint copy this far behind the drop */
#define RAIN_TAIL_LVL     90
#define SPLASH_REACH      2     /* LEDs either side the splash spreads */
#define SPLASH_STEP_MS    40    /* delay per LED of spread */
#define SPLASH_ATTACK_MS  12
#define SPLASH_DECAY_MS   70
#define MAX_DROPS         6
#define MAX_SPLASHES      4
/* ---------------------------------------------------------------------- */

static const uint8_t SPLASH_AMP[SPLASH_REACH + 1] = {255, 200, 130};

typedef struct {
    uint8_t on;
    int8_t side;      /* +1 = falls down the right side (clockwise), -1 = left */
    uint32_t s_q8;    /* distance fallen from the top, ring units Q8 */
    uint32_t v_q8;    /* current speed, ring units/ms Q8 */
} drop_t;

static drop_t drops[MAX_DROPS];
static int32_t splash_t[MAX_SPLASHES]; /* ms since splash, <0 = unused */
static int32_t next_drop_ms;
static uint32_t rng;

static void init(uint32_t seed)
{
    rng = xs_seed(seed);
    for (int i = 0; i < MAX_DROPS; i++) drops[i].on = 0;
    for (int i = 0; i < MAX_SPLASHES; i++) splash_t[i] = -1;
    next_drop_ms = 100;
}

/* Gravity: speed = VMAX * sqrt((1 - height) / 2), height = cos(angle). */
static uint32_t speed_q8(uint32_t s)
{
    int32_t h = cos16(ring_to_angle(s));          /* Q15, +1 at the top */
    uint32_t q = (uint32_t)(32767 - h) / 2U;      /* (1 - h) / 2, Q15 */
    uint32_t root = isqrt32(q << 15);             /* sqrt(...), Q15 */
    return (RAIN_VMAX * 256U * root) >> 15;
}

static void splash(void)
{
    int oldest = 0;
    for (int i = 0; i < MAX_SPLASHES; i++) {
        if (splash_t[i] < 0) { oldest = i; break; }
        if (splash_t[i] > splash_t[oldest]) oldest = i;
    }
    splash_t[oldest] = 0;
}

static void update(uint32_t frame_ms, uint8_t out[RING_LEDS])
{
    /* Spawn. */
    next_drop_ms -= (int32_t)frame_ms;
    if (next_drop_ms <= 0) {
        next_drop_ms = (int32_t)xs_range(&rng, RAIN_GAP_MIN_MS, RAIN_GAP_MAX_MS);
        for (int i = 0; i < MAX_DROPS; i++) {
            if (!drops[i].on) {
                drops[i].on = 1;
                drops[i].side = (xs32(&rng) & 1U) ? 1 : -1;
                drops[i].s_q8 = RAIN_START_Q8 << 8;
                drops[i].v_q8 = 0;
                break;
            }
        }
    }

    fx_buf_t buf;
    fx_clear(buf);

    /* Fall. */
    for (int i = 0; i < MAX_DROPS; i++) {
        drop_t *d = &drops[i];
        if (!d->on) continue;
        d->v_q8 = speed_q8(d->s_q8 >> 8);
        d->s_q8 += d->v_q8 * frame_ms;
        uint32_t s = d->s_q8 >> 8;
        if (s >= RING_POS_BOTTOM) {
            d->on = 0;
            splash();
            continue;
        }
        uint32_t back = (d->v_q8 * RAIN_TAIL_MS) >> 8;
        uint32_t tail = s > back ? s - back : 0;
        uint32_t head_pos = d->side > 0 ? s : ring_wrap(-(int32_t)s);
        uint32_t tail_pos = d->side > 0 ? tail : ring_wrap(-(int32_t)tail);
        fx_point(buf, tail_pos, RAIN_TAIL_LVL);
        fx_point(buf, head_pos, RAIN_PEAK);
    }

    /* Splashes: flash at the bottom LED spreading sideways. */
    for (int k = 0; k < MAX_SPLASHES; k++) {
        if (splash_t[k] < 0) continue;
        splash_t[k] += (int32_t)frame_ms;
        int alive = 0;
        for (int r = 0; r <= SPLASH_REACH; r++) {
            uint32_t e = fx_envelope(splash_t[k] - r * SPLASH_STEP_MS,
                                     SPLASH_ATTACK_MS, SPLASH_DECAY_MS);
            uint32_t v = (SPLASH_AMP[r] * e) >> 16;
            alive |= v > 0 || splash_t[k] < r * SPLASH_STEP_MS;
            fx_max(buf, ring_led_at(RING_LEDS / 2 + r), v);
            fx_max(buf, ring_led_at(RING_LEDS / 2 - r), v);
        }
        if (!alive) splash_t[k] = -1;
    }
    fx_out(buf, out);
}

const mode_def_t mode_rain = {"raindrops", init, update};
