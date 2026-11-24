/*
 * mode_pendulum.c - 5. Pendulum: a bright dot hanging from the top swings
 * side to side around the bottom like a real pendulum: sinusoidal angle,
 * slow at the ends, fastest at the bottom, with a short motion-blur tail.
 */
#include "mode.h"
#include "fx.h"

/* ---- Tweakable ------------------------------------------------------- */
#define PEND_PERIOD_MS    2400  /* one full swing there and back */
#define PEND_AMP_DEG      70    /* swing amplitude either side of the bottom */
#define PEND_RADIUS_Q8    400   /* dot half-width, 1/256 LED (400 = 1.6 LEDs) */
#define PEND_PEAK         255
#define PEND_BLUR_SAMPLES 5     /* motion-blur copies */
#define PEND_BLUR_STEP_MS 20    /* time between copies */
#define PEND_BLUR_KEEP    150   /* each older copy keeps this/255 brightness */
/* ---------------------------------------------------------------------- */

#define AMP_POS ((int32_t)RING_POS * PEND_AMP_DEG / 360)

static uint32_t t_ms;

/* Bob position at time t (may be "negative" via wraparound of the period). */
static uint32_t bob_pos(int32_t t)
{
    int32_t tm = t % PEND_PERIOD_MS;
    if (tm < 0) tm += PEND_PERIOD_MS;
    uint16_t ang = (uint16_t)(((uint32_t)tm << 16) / PEND_PERIOD_MS);
    int32_t off = (AMP_POS * sin16(ang)) >> 15;
    return ring_wrap(RING_POS_BOTTOM + off);
}

static void init(uint32_t seed)
{
    (void)seed;
    t_ms = PEND_PERIOD_MS / 4; /* start at the right-hand end of the swing */
}

static void update(uint32_t frame_ms, uint8_t out[RING_LEDS])
{
    t_ms = (t_ms + frame_ms) % PEND_PERIOD_MS;
    const uint32_t r = PEND_RADIUS_Q8 * RING_POS_PER_LED / 256;

    fx_buf_t buf;
    fx_clear(buf);
    /* Oldest copy first; max-combine so copies don't stack up at the ends. */
    uint32_t v = PEND_PEAK;
    uint8_t lvl[PEND_BLUR_SAMPLES + 1];
    for (int k = 0; k <= PEND_BLUR_SAMPLES; k++) {
        lvl[k] = (uint8_t)v;
        v = v * PEND_BLUR_KEEP / 255U;
    }
    for (int k = PEND_BLUR_SAMPLES; k >= 0; k--) {
        fx_blob_max(buf, bob_pos((int32_t)t_ms - k * PEND_BLUR_STEP_MS), r, lvl[k]);
    }
    fx_out(buf, out);
}

const mode_def_t mode_pendulum = {"pendulum", init, update};
