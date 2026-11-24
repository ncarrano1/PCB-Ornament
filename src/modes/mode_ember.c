/*
 * mode_ember.c - 20. Ember: a few LEDs at the bottom breathe very slowly at
 * low brightness, drifting a little. A low-power mode for leaving the
 * ornament on all evening (about 0.5 mA of LED current).
 */
#include "mode.h"
#include "fx.h"

/* ---- Tweakable ------------------------------------------------------- */
#define EMBER_BREATH_MS 6500  /* one slow breath */
#define EMBER_MIN       45    /* perceptual brightness at the bottom of a breath */
#define EMBER_MAX       120   /* ... and at the top */
#define EMBER_RADIUS_Q8 680   /* glow half-width, 1/256 LED (~2.7 LEDs) */
#define EMBER_DRIFT_Q8  110   /* slow wander either side of the bottom */
#define EMBER_DRIFT_MS  23000
/* ---------------------------------------------------------------------- */

static uint32_t t_ms;

static void init(uint32_t seed)
{
    (void)seed;
    t_ms = EMBER_BREATH_MS / 2; /* fade in from the dim end */
}

static void update(uint32_t frame_ms, uint8_t out[RING_LEDS])
{
    t_ms += frame_ms;
    uint16_t b = (uint16_t)(((uint64_t)(t_ms % EMBER_BREATH_MS) << 16) / EMBER_BREATH_MS);
    uint16_t d = (uint16_t)(((uint64_t)(t_ms % EMBER_DRIFT_MS) << 16) / EMBER_DRIFT_MS);
    /* (1 - cos) / 2: 0 -> 1 -> 0 over a breath */
    uint32_t breath = (uint32_t)(32768 - cos16(b)) >> 1;  /* Q15 */
    uint32_t lvl = EMBER_MIN + (((EMBER_MAX - EMBER_MIN) * breath) >> 15);
    int32_t drift = (EMBER_DRIFT_Q8 * RING_POS_PER_LED / 256 * sin16(d)) >> 15;

    fx_buf_t buf;
    fx_clear(buf);
    fx_blob(buf, ring_wrap(RING_POS_BOTTOM + drift), EMBER_RADIUS_Q8 * RING_POS_PER_LED / 256, (uint8_t)lvl);
    fx_out(buf, out);
}

const mode_def_t mode_ember = {"ember", init, update};
