/*
 * mode_binary.c - 3. Binary stars: two comets orbiting in opposite
 * directions, with a brief flare where they pass through each other.
 */
#include "mode.h"
#include "fx.h"

/* ---- Tweakable ------------------------------------------------------- */
#define STAR_A_MS       5000  /* clockwise star, turn time */
#define STAR_B_MS       6500  /* counter-clockwise star, turn time */
#define STAR_TAIL_LEDS  2     /* tail e-folding length */
#define STAR_TAIL_MAX   8
#define STAR_PEAK       230
#define FLARE_GAP_LEDS  2     /* flare starts when the stars are this close */
#define FLARE_RADIUS    4     /* flare blob half-width, LEDs */
#define FLARE_DECAY_MS  200   /* flare fade time constant */
/* ---------------------------------------------------------------------- */

#define VA ((uint32_t)(((uint64_t)RING_POS << 16) / STAR_A_MS))
#define VB ((uint32_t)(((uint64_t)RING_POS << 16) / STAR_B_MS))
#define RING_Q16 ((uint64_t)RING_POS << 16)

static uint32_t a_q16, b_q16;
static uint32_t flare;      /* 0..65535 */
static uint32_t flare_pos;

static void init(uint32_t seed)
{
    (void)seed;
    a_q16 = 0;
    b_q16 = (uint32_t)(RING_Q16 / 2); /* start on opposite sides */
    flare = 0;
    flare_pos = 0;
}

static void update(uint32_t frame_ms, uint8_t out[RING_LEDS])
{
    a_q16 = (uint32_t)(((uint64_t)a_q16 + (uint64_t)VA * frame_ms) % RING_Q16);
    b_q16 = (uint32_t)(((uint64_t)b_q16 + RING_Q16 - ((uint64_t)VB * frame_ms) % RING_Q16) % RING_Q16);
    uint32_t a = a_q16 >> 16, b = b_q16 >> 16;

    /* Flare: decays exponentially, re-ignited by closeness. */
    flare = (flare * exp_neg(frame_ms * 256U / FLARE_DECAY_MS)) >> 16;
    uint32_t gap = ring_dist(a, b);
    const uint32_t gap_max = FLARE_GAP_LEDS * RING_POS_PER_LED;
    if (gap < gap_max) {
        uint32_t target = (gap_max - gap) * 65535U / gap_max;
        if (target > flare) {
            flare = target;
            /* midpoint along the short arc */
            uint32_t cw = ring_cw(a, b);
            flare_pos = cw <= RING_POS / 2 ? (a + cw / 2) % RING_POS
                                           : (b + (RING_POS - cw) / 2) % RING_POS;
        }
    }

    fx_buf_t buf;
    fx_clear(buf);
    fx_comet(buf, a, +1, STAR_TAIL_LEDS, STAR_TAIL_MAX, STAR_PEAK);
    fx_comet(buf, b, -1, STAR_TAIL_LEDS, STAR_TAIL_MAX, STAR_PEAK);
    if (flare > 0) {
        uint32_t r = FLARE_RADIUS * RING_POS_PER_LED * flare / 65535U;
        fx_blob(buf, flare_pos, r, (uint8_t)(255U * flare / 65535U));
    }
    fx_out(buf, out);
}

const mode_def_t mode_binary = {"binary stars", init, update};
