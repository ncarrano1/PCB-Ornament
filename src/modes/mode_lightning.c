/*
 * mode_lightning.c - 15. Lightning storm: mostly dark. A stepped leader
 * jerks down from the top, then a bright return stroke lights its path,
 * often followed by a few re-strikes. Sometimes it forks down both sides;
 * between strikes the "clouds" at the top flicker faintly.
 */
#include "mode.h"
#include "fx.h"

/* ---- Tweakable ------------------------------------------------------- */
#define LT_GAP_MIN_MS    1800  /* time between strikes */
#define LT_GAP_MAX_MS    6500
#define LT_REACH_MIN     8     /* how far down the bolt reaches, LEDs */
#define LT_REACH_MAX     20
#define LT_FORK_PCT      35    /* chance of forking down both sides */
#define LT_STEP_MIN_MS   12    /* leader: time per jerk */
#define LT_STEP_MAX_MS   35
#define LT_JUMP_MAX      3     /* leader: LEDs per jerk */
#define LT_LEADER_LVL    70
#define LT_FLASH_DECAY   70    /* stroke fade, ms */
#define LT_RESTRIKE_MAX  3
#define LT_RESTRIKE_MIN_MS 50
#define LT_RESTRIKE_MAX_MS 150
#define LT_CLOUD_PCT     45    /* chance of a cloud flicker during a gap */
#define LT_CLOUD_LEDS    3     /* cloud flicker reach either side of the top */
/* ---------------------------------------------------------------------- */

typedef enum { LT_IDLE, LT_LEADER, LT_STROKE } lt_state_t;

static lt_state_t state;
static int32_t timer;       /* ms to the next event */
static uint8_t sides;       /* bit0: right side, bit1: left side */
static uint32_t reach;      /* ring units */
static uint32_t lead;       /* leader progress, ring units */
static uint32_t flash_t;    /* ms since the last (re)strike */
static uint8_t flash_peak;
static int restrikes;
static int32_t cloud_at;    /* ms into the gap when the cloud flickers, <0 none */
static uint32_t cloud_t;
static uint32_t rng;

static void start_gap(void)
{
    state = LT_IDLE;
    timer = (int32_t)xs_range(&rng, LT_GAP_MIN_MS, LT_GAP_MAX_MS);
    cloud_at = xs_range(&rng, 0, 99) < LT_CLOUD_PCT ? (int32_t)xs_range(&rng, 200, (uint32_t)timer) : -1;
    cloud_t = 0;
}

static void init(uint32_t seed)
{
    rng = xs_seed(seed);
    start_gap();
    timer = 800;
}

/* Draw the bolt path from the top down to `len` at brightness v. */
static void path(fx_buf_t buf, uint32_t len, uint8_t v, int brighter_top)
{
    for (int i = 0; i < RING_LEDS; i++) {
        uint32_t p = ring_led_pos(i);
        int right = p <= RING_POS / 2;
        if (!(sides & (right ? 1 : 2))) continue;
        uint32_t d = ring_dist(p, RING_POS_TOP);
        if (d > len) continue;
        uint32_t lvl = v;
        if (brighter_top) lvl = lvl * (256U - 80U * d / (len + 1U)) / 256U;
        fx_max(buf, i, lvl);
    }
}

static void update(uint32_t frame_ms, uint8_t out[RING_LEDS])
{
    fx_buf_t buf;
    fx_clear(buf);
    timer -= (int32_t)frame_ms;

    if (state == LT_IDLE) {
        if (cloud_at >= 0) {
            cloud_t += frame_ms;
            int32_t ct = (int32_t)cloud_t - cloud_at;
            uint32_t e = fx_envelope(ct, 10, 60) + fx_envelope(ct - 140, 10, 50) / 2U;
            if (e > 65535U) e = 65535U;
            fx_arc(buf, RING_POS_TOP, LT_CLOUD_LEDS * RING_POS_PER_LED, (uint8_t)((90U * e) >> 16));
        }
        if (timer <= 0) {
            state = LT_LEADER;
            uint32_t r = xs_range(&rng, 0, 99);
            sides = r < LT_FORK_PCT ? 3 : (r & 1U) ? 1 : 2;
            reach = (uint32_t)xs_range(&rng, LT_REACH_MIN, LT_REACH_MAX) * RING_POS_PER_LED;
            lead = 0;
            timer = 0;
        }
    }

    if (state == LT_LEADER) {
        if (timer <= 0) {
            lead += (uint32_t)xs_range(&rng, 1, LT_JUMP_MAX) * RING_POS_PER_LED;
            timer = (int32_t)xs_range(&rng, LT_STEP_MIN_MS, LT_STEP_MAX_MS);
        }
        if (lead >= reach) {
            state = LT_STROKE;
            flash_t = 0;
            flash_peak = 255;
            restrikes = (int)xs_range(&rng, 0, LT_RESTRIKE_MAX);
            timer = (int32_t)xs_range(&rng, LT_RESTRIKE_MIN_MS, LT_RESTRIKE_MAX_MS);
        } else {
            path(buf, lead, LT_LEADER_LVL, 0);
            /* bright tip */
            uint32_t tip = lead;
            if (sides & 1) fx_point(buf, tip, 190);
            if (sides & 2) fx_point(buf, ring_wrap(-(int32_t)tip), 190);
        }
    }

    if (state == LT_STROKE) {
        flash_t += frame_ms;
        uint32_t e = exp_neg(flash_t * 256U / LT_FLASH_DECAY);
        path(buf, reach, (uint8_t)((flash_peak * e) >> 16), 1);
        if (timer <= 0) {
            if (restrikes > 0) {
                restrikes--;
                flash_t = 0;
                flash_peak = (uint8_t)xs_range(&rng, 150, 255);
                timer = (int32_t)xs_range(&rng, LT_RESTRIKE_MIN_MS, LT_RESTRIKE_MAX_MS);
            } else if (e < 1000) {
                start_gap();
            }
        }
    }
    fx_out(buf, out);
}

const mode_def_t mode_lightning = {"lightning", init, update};
