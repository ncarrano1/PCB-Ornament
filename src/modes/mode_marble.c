/*
 * mode_marble.c - 12. Marble in a bowl: a ball rolls around the bottom of
 * the ring under gravity, losing energy to friction until it settles at the
 * bottom with a soft glow. After a pause it gets a random kick up one side,
 * sometimes hard enough to loop the whole ring.
 */
#include "mode.h"
#include "fx.h"

/* ---- Tweakable ------------------------------------------------------- */
#define MARBLE_G_Q12        51    /* gravity, ring units/ms^2 * 4096 */
#define MARBLE_DAMP_MS      3000  /* rolling drag time constant */
#define MARBLE_STICTION_Q12 4     /* constant friction, units/ms^2 * 4096 */
#define MARBLE_KICK_MIN     4     /* kick speed, ring units/ms (9+ loops the ring) */
#define MARBLE_KICK_MAX     11
#define MARBLE_PAUSE_MIN_MS 1200  /* rest time before the next kick */
#define MARBLE_PAUSE_MAX_MS 2600
#define MARBLE_RADIUS_Q8    330   /* ball half-width, 1/256 LED */
#define MARBLE_PEAK         255
#define MARBLE_BLUR         3     /* motion-blur copies (2 frames apart) */
#define MARBLE_GLOW         120   /* glow when it comes to rest */
/* ---------------------------------------------------------------------- */

#define RING_Q12   ((int32_t)RING_POS * 4096)
#define HIST       (MARBLE_BLUR * 2 + 1)

static int32_t pos_q12, vel_q12;
static uint32_t hist[HIST];
static int resting;
static uint32_t still_ms, rest_ms;
static int32_t wait_ms;
static uint32_t rng;

static void kick(void)
{
    int32_t v = (int32_t)xs_range(&rng, MARBLE_KICK_MIN * 4096U, MARBLE_KICK_MAX * 4096U);
    vel_q12 = (xs32(&rng) & 1U) ? v : -v;
    resting = 0;
    still_ms = 0;
}

static void init(uint32_t seed)
{
    rng = xs_seed(seed);
    pos_q12 = RING_POS_BOTTOM * 4096;
    for (int k = 0; k < HIST; k++) hist[k] = RING_POS_BOTTOM;
    kick();
}

static void update(uint32_t frame_ms, uint8_t out[RING_LEDS])
{
    const int32_t dt = (int32_t)frame_ms;
    uint32_t pos = (uint32_t)pos_q12 >> 12;

    if (!resting) {
        vel_q12 += fx_gravity(pos, MARBLE_G_Q12) * dt;
        vel_q12 -= vel_q12 * dt / MARBLE_DAMP_MS;
        int32_t st = MARBLE_STICTION_Q12 * dt;
        vel_q12 = vel_q12 > st ? vel_q12 - st : vel_q12 < -st ? vel_q12 + st : 0;
        pos_q12 += vel_q12 * dt;
        pos_q12 %= RING_Q12;
        if (pos_q12 < 0) pos_q12 += RING_Q12;
        pos = (uint32_t)pos_q12 >> 12;

        /* Settled: slow and near the bottom for a while. */
        int32_t av = vel_q12 < 0 ? -vel_q12 : vel_q12;
        if (av < 1200 && ring_dist(pos, RING_POS_BOTTOM) < RING_POS_PER_LED / 2) {
            still_ms += frame_ms;
        } else {
            still_ms = 0;
        }
        if (still_ms >= 250) {
            resting = 1;
            rest_ms = 0;
            wait_ms = (int32_t)xs_range(&rng, MARBLE_PAUSE_MIN_MS, MARBLE_PAUSE_MAX_MS);
        }
    } else {
        /* Ease the last bit onto the bottom, then wait for the next kick. */
        int32_t d = RING_POS_BOTTOM * 4096 - pos_q12;
        pos_q12 += d * dt / 150;
        pos = (uint32_t)pos_q12 >> 12;
        rest_ms += frame_ms;
        wait_ms -= dt;
        if (wait_ms <= 0) kick();
    }

    for (int k = HIST - 1; k > 0; k--) hist[k] = hist[k - 1];
    hist[0] = pos;

    fx_buf_t buf;
    fx_clear(buf);
    const uint32_t r = MARBLE_RADIUS_Q8 * RING_POS_PER_LED / 256;
    uint32_t lvl = MARBLE_PEAK;
    uint8_t lv[MARBLE_BLUR + 1];
    for (int k = 0; k <= MARBLE_BLUR; k++) { lv[k] = (uint8_t)lvl; lvl = lvl * 130U / 255U; }
    for (int k = MARBLE_BLUR; k >= 0; k--) fx_blob_max(buf, hist[k * 2], r, lv[k]);

    if (resting) {
        /* Soft glow spreading out under the ball as it settles. */
        uint32_t e = fx_envelope((int32_t)rest_ms, 80, 350);
        fx_blob(buf, RING_POS_BOTTOM, 3 * RING_POS_PER_LED, (uint8_t)((MARBLE_GLOW * e) >> 16));
    }
    fx_out(buf, out);
}

const mode_def_t mode_marble = {"marble", init, update};
