/*
 * mode_ripples.c - 17. Pond ripples: drops land at random places on the
 * ring; each sends a ripple both ways that widens and fades as it travels.
 * Ripples pass through each other and add up where they cross.
 */
#include "mode.h"
#include "fx.h"

/* ---- Tweakable ------------------------------------------------------- */
#define RP_GAP_MIN_MS   500   /* time between drops */
#define RP_GAP_MAX_MS   1600
#define RP_SPEED_Q8     655   /* ripple speed, ring units/ms * 256 (~10 LEDs/s) */
#define RP_DECAY_MS     2000  /* fade time constant */
#define RP_WIDTH0       300   /* starting half-width of a ripple, ring units */
#define RP_WIDEN        120   /* extra half-width per second */
#define RP_PEAK         250
#define RP_SPLASH_MS    110   /* bright splash where the drop lands */
#define RP_MAX          6
/* ---------------------------------------------------------------------- */

typedef struct {
    uint8_t on;
    uint32_t origin;
    uint32_t age;
    uint8_t peak;
} ripple_t;

static ripple_t rp[RP_MAX];
static int32_t next_ms;
static uint32_t rng;

static void init(uint32_t seed)
{
    rng = xs_seed(seed);
    for (int i = 0; i < RP_MAX; i++) rp[i].on = 0;
    next_ms = 200;
}

static void update(uint32_t frame_ms, uint8_t out[RING_LEDS])
{
    next_ms -= (int32_t)frame_ms;
    if (next_ms <= 0) {
        next_ms = (int32_t)xs_range(&rng, RP_GAP_MIN_MS, RP_GAP_MAX_MS);
        int slot = 0;
        for (int i = 0; i < RP_MAX; i++) {
            if (!rp[i].on) { slot = i; break; }
            if (rp[i].age > rp[slot].age) slot = i; /* reuse the oldest */
        }
        rp[slot].on = 1;
        rp[slot].origin = (uint32_t)xs_range(&rng, 0, RING_POS - 1);
        rp[slot].age = 0;
        rp[slot].peak = (uint8_t)xs_range(&rng, 200, RP_PEAK);
    }

    fx_buf_t buf;
    fx_clear(buf);
    for (int i = 0; i < RP_MAX; i++) {
        ripple_t *r = &rp[i];
        if (!r->on) continue;
        r->age += frame_ms;
        uint32_t amp = (r->peak * (uint32_t)exp_neg(r->age * 256U / RP_DECAY_MS)) >> 16;
        if (amp < 20) {
            r->on = 0;
            continue;
        }
        uint32_t dist = (RP_SPEED_Q8 * r->age) >> 8;
        uint32_t w = RP_WIDTH0 + RP_WIDEN * r->age / 1000U;
        if (r->age < RP_SPLASH_MS * 4) {
            uint32_t e = fx_envelope((int32_t)r->age, 15, RP_SPLASH_MS);
            fx_blob(buf, r->origin, RING_POS_PER_LED * 3 / 2, (uint8_t)((255U * e) >> 16));
        }
        if (dist > 0 && dist < RING_POS) {
            fx_blob(buf, (r->origin + dist) % RING_POS, w, (uint8_t)amp);
            fx_blob(buf, ring_wrap((int32_t)r->origin - (int32_t)dist), w, (uint8_t)amp);
        }
    }
    fx_out(buf, out);
}

const mode_def_t mode_ripples = {"ripples", init, update};
