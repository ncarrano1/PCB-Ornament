/*
 * mode_comet.c - 2. Comet: a bright head with an exponentially fading
 * tail, circling clockwise, slowly speeding up and slowing down.
 */
#include "mode.h"
#include "fx.h"

/* ---- Tweakable ------------------------------------------------------- */
#define COMET_FAST_MS   1500   /* turn time at top speed */
#define COMET_SLOW_MS   4000   /* turn time at lowest speed */
#define COMET_CYCLE_MS  12000  /* one speed-up/slow-down cycle */
#define COMET_TAIL_LEDS 4      /* e-folding length of the tail (perceptual) */
#define COMET_TAIL_MAX  14     /* tail cut-off, LEDs */
#define COMET_PEAK      255
/* ---------------------------------------------------------------------- */

/* Speeds in ring units per ms, Q16. */
#define V_FAST ((uint32_t)(((uint64_t)RING_POS << 16) / COMET_FAST_MS))
#define V_SLOW ((uint32_t)(((uint64_t)RING_POS << 16) / COMET_SLOW_MS))

static uint32_t head_q16; /* position, Q16 ring units */
static uint32_t t_ms;

static void init(uint32_t seed)
{
    (void)seed;
    head_q16 = 0;
    t_ms = 0;
}

static void update(uint32_t frame_ms, uint8_t out[RING_LEDS])
{
    t_ms += frame_ms;
    /* speed = mid + amp * sin: slow -> fast -> slow over COMET_CYCLE_MS */
    uint16_t ang = (uint16_t)(((uint64_t)(t_ms % COMET_CYCLE_MS) << 16) / COMET_CYCLE_MS);
    int32_t s = sin16((uint16_t)(ang - 16384U)); /* start at the slow end */
    uint32_t mid = (V_FAST + V_SLOW) / 2, amp = (V_FAST - V_SLOW) / 2;
    uint32_t v = (uint32_t)((int32_t)mid + (int32_t)(((int64_t)amp * s) >> 15));
    head_q16 = (uint32_t)(((uint64_t)head_q16 + (uint64_t)v * frame_ms) % ((uint64_t)RING_POS << 16));

    fx_buf_t buf;
    fx_clear(buf);
    fx_comet(buf, head_q16 >> 16, +1, COMET_TAIL_LEDS, COMET_TAIL_MAX, COMET_PEAK);
    fx_out(buf, out);
}

const mode_def_t mode_comet = {"comet", init, update};
