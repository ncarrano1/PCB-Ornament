/*
 * mode_fireflies.c - 16. Fireflies: a few lights wander slowly around the
 * ring, each blinking with its own rhythm. Whenever one flashes, the others
 * nudge their timing forward (pulse coupling, as real fireflies do), so
 * over half a minute or so they fall into unison. Every so often something
 * startles them and they scatter out of sync again.
 */
#include "mode.h"
#include "fx.h"

/* ---- Tweakable ------------------------------------------------------- */
#define FF_COUNT          5
#define FF_PERIOD_MIN_MS  2700  /* each firefly's natural blink period */
#define FF_PERIOD_MAX_MS  2950
#define FF_FLASH_PCT      22    /* share of the cycle spent glowing */
#define FF_COUPLING_PCT   13    /* how strongly a flash pulls the others */
#define FF_SCATTER_MIN_MS 35000 /* time between "startles" */
#define FF_SCATTER_MAX_MS 60000
#define FF_WANDER_Q12     3     /* random push, ring units/ms^2 * 4096 */
#define FF_MAX_SPEED_Q12  900   /* top wander speed, ring units/ms * 4096 */
#define FF_RADIUS_Q8      300
#define FF_PEAK           255
/* ---------------------------------------------------------------------- */

#define RING_Q12 ((int32_t)RING_POS * 4096)
#define FLASH_END ((uint32_t)(4294967296ULL * FF_FLASH_PCT / 100))

typedef struct {
    int32_t pos_q12, vel_q12;
    uint32_t phase;  /* 2^32 = one blink cycle; flashing while < FLASH_END */
    uint32_t step;   /* phase per ms */
} fly_t;

static fly_t fly[FF_COUNT];
static int32_t scatter_ms;
static uint32_t rng;

static void scatter(void)
{
    for (int k = 0; k < FF_COUNT; k++) fly[k].phase = xs32(&rng);
    scatter_ms = (int32_t)xs_range(&rng, FF_SCATTER_MIN_MS, FF_SCATTER_MAX_MS);
}

static void init(uint32_t seed)
{
    rng = xs_seed(seed);
    for (int k = 0; k < FF_COUNT; k++) {
        fly[k].pos_q12 = (int32_t)xs_range(&rng, 0, RING_POS - 1) * 4096;
        fly[k].vel_q12 = 0;
        fly[k].step = (uint32_t)(4294967296ULL / xs_range(&rng, FF_PERIOD_MIN_MS, FF_PERIOD_MAX_MS));
    }
    scatter();
}

static void update(uint32_t frame_ms, uint8_t out[RING_LEDS])
{
    scatter_ms -= (int32_t)frame_ms;
    if (scatter_ms <= 0) scatter();

    /* Advance the blink clocks; note who starts a flash this frame. */
    int fired = 0;
    for (int k = 0; k < FF_COUNT; k++) {
        uint32_t before = fly[k].phase;
        fly[k].phase += fly[k].step * frame_ms;
        if (fly[k].phase < before) fired |= 1 << k; /* wrapped: new flash */
    }
    /* Pulse coupling: each flash pushes the others' clocks forward. */
    for (int j = 0; j < FF_COUNT; j++) {
        if (!(fired & (1 << j))) continue;
        for (int k = 0; k < FF_COUNT; k++) {
            if (k == j || (fired & (1 << k)) || fly[k].phase < FLASH_END) continue;
            uint64_t p = fly[k].phase + (uint64_t)fly[k].phase * FF_COUPLING_PCT / 100U;
            fly[k].phase = p >= 4294967296ULL ? 0U : (uint32_t)p; /* pushed over: flash now */
        }
    }

    fx_buf_t buf;
    fx_clear(buf);
    for (int k = 0; k < FF_COUNT; k++) {
        fly_t *f = &fly[k];
        /* Wander with momentum. */
        int32_t push = (int32_t)xs_range(&rng, 0, 2U * FF_WANDER_Q12) - FF_WANDER_Q12;
        f->vel_q12 += push * (int32_t)frame_ms;
        if (f->vel_q12 > FF_MAX_SPEED_Q12) f->vel_q12 = FF_MAX_SPEED_Q12;
        if (f->vel_q12 < -FF_MAX_SPEED_Q12) f->vel_q12 = -FF_MAX_SPEED_Q12;
        f->pos_q12 = (f->pos_q12 + f->vel_q12 * (int32_t)frame_ms) % RING_Q12;
        if (f->pos_q12 < 0) f->pos_q12 += RING_Q12;

        if (f->phase < FLASH_END) {
            /* slow swell and fade: half sine over the flash */
            uint16_t a = (uint16_t)(((uint64_t)f->phase << 15) / FLASH_END);
            int32_t s = sin16(a);
            uint32_t v = (FF_PEAK * (uint32_t)(s < 0 ? 0 : s)) >> 15;
            fx_blob_max(buf, (uint32_t)f->pos_q12 >> 12, FF_RADIUS_Q8 * RING_POS_PER_LED / 256, (uint8_t)v);
        }
    }
    fx_out(buf, out);
}

const mode_def_t mode_fireflies = {"fireflies", init, update};
