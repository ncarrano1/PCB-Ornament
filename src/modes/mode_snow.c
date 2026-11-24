/*
 * mode_snow.c - 13. Snowfall: flakes drift slowly down both sides with a
 * gentle side-to-side wobble and pile up at the bottom. When the pile is
 * tall enough it melts away from the top down, then the snow starts again.
 */
#include "mode.h"
#include "fx.h"

/* ---- Tweakable ------------------------------------------------------- */
#define SNOW_GAP_MIN_MS   350   /* time between new flakes */
#define SNOW_GAP_MAX_MS   1000
#define SNOW_SPEED_Q8     262   /* fall speed, ring units/ms * 256 (~4 LEDs/s) */
#define SNOW_SPEED_VAR    35    /* +- % per flake */
#define SNOW_WOBBLE       150   /* side-to-side wobble, ring units (~0.6 LED) */
#define SNOW_WOBBLE_MIN_MS 900  /* wobble period range */
#define SNOW_WOBBLE_MAX_MS 1900
#define SNOW_PEAK_MIN     140
#define SNOW_PEAK_MAX     230
#define PILE_LVL          130   /* brightness of the settled snow */
#define PILE_ADD          85    /* pile growth per flake, ring units (each side) */
#define PILE_MAX_LEDS     6     /* melt when the pile reaches this far up each side */
#define MELT_MS           7000
#define MAX_FLAKES        8
/* ---------------------------------------------------------------------- */

typedef struct {
    uint8_t on;
    int8_t side;
    uint8_t peak;
    uint32_t s_q8;      /* distance fallen from the top, ring units * 256 */
    uint32_t v_q8;
    uint16_t wob;       /* wobble phase */
    uint16_t wob_step;  /* phase step per ms */
} flake_t;

static flake_t fl[MAX_FLAKES];
static uint32_t pile_q8;   /* pile reach from the bottom, ring units * 256 */
static int melting;
static int32_t next_ms;
static uint32_t rng;

static void init(uint32_t seed)
{
    rng = xs_seed(seed);
    for (int i = 0; i < MAX_FLAKES; i++) fl[i].on = 0;
    pile_q8 = 0;
    melting = 0;
    next_ms = 0;
}

static void spawn(void)
{
    for (int i = 0; i < MAX_FLAKES; i++) {
        flake_t *f = &fl[i];
        if (f->on) continue;
        f->on = 1;
        f->side = (xs32(&rng) & 1U) ? 1 : -1;
        f->peak = (uint8_t)xs_range(&rng, SNOW_PEAK_MIN, SNOW_PEAK_MAX);
        f->s_q8 = (uint32_t)xs_range(&rng, 0, RING_POS_PER_LED) << 8;
        f->v_q8 = SNOW_SPEED_Q8 * (uint32_t)xs_range(&rng, 100 - SNOW_SPEED_VAR, 100 + SNOW_SPEED_VAR) / 100U;
        f->wob = (uint16_t)xs32(&rng);
        f->wob_step = (uint16_t)(65536U / xs_range(&rng, SNOW_WOBBLE_MIN_MS, SNOW_WOBBLE_MAX_MS));
        return;
    }
}

static void update(uint32_t frame_ms, uint8_t out[RING_LEDS])
{
    const uint32_t pile_max = (uint32_t)PILE_MAX_LEDS * RING_POS_PER_LED << 8;

    if (!melting) {
        next_ms -= (int32_t)frame_ms;
        if (next_ms <= 0) {
            next_ms = (int32_t)xs_range(&rng, SNOW_GAP_MIN_MS, SNOW_GAP_MAX_MS);
            spawn();
        }
    } else {
        uint32_t melt = (uint32_t)(((uint64_t)pile_max * frame_ms) / MELT_MS);
        pile_q8 = pile_q8 > melt ? pile_q8 - melt : 0;
        if (pile_q8 == 0) melting = 0;
    }

    fx_buf_t buf;
    fx_clear(buf);

    /* Settled snow, with a slight shimmer. */
    if (pile_q8 > 0) {
        fx_arc(buf, RING_POS_BOTTOM, pile_q8 >> 8, PILE_LVL);
    }

    for (int i = 0; i < MAX_FLAKES; i++) {
        flake_t *f = &fl[i];
        if (!f->on) continue;
        f->s_q8 += f->v_q8 * frame_ms;
        f->wob = (uint16_t)(f->wob + f->wob_step * frame_ms);
        uint32_t s = f->s_q8 >> 8;
        /* Landed on the pile (or the bottom while melting)? */
        uint32_t top_of_pile = RING_POS_BOTTOM - (pile_q8 >> 8);
        if (s >= top_of_pile) {
            f->on = 0;
            if (!melting) {
                pile_q8 += (uint32_t)PILE_ADD << 8;
                if (pile_q8 >= pile_max) {
                    pile_q8 = pile_max;
                    melting = 1;
                }
            }
            continue;
        }
        int32_t w = (SNOW_WOBBLE * sin16(f->wob)) >> 15;
        int32_t arc = (int32_t)s + w;
        uint32_t pos = ring_wrap(f->side > 0 ? arc : -arc);
        fx_point(buf, pos, f->peak);
    }
    fx_out(buf, out);
}

const mode_def_t mode_snow = {"snowfall", init, update};
