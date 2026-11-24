/*
 * gauge.c - battery gauge shown at power-up and wake. Pure C.
 */
#include "gauge.h"
#include "fx.h"

/* ---- Tweakable ------------------------------------------------------- */
#define GAUGE_RISE_MS   700   /* level rises */
#define GAUGE_HOLD_MS   1300  /* level holds */
#define GAUGE_FADE_MS   400   /* mode fades in over it */
#define GAUGE_LVL       210   /* filled part */
#define GAUGE_SCALE_LVL 40    /* faint full ring, so the scale is visible */
#define GAUGE_LOW_PCT   20    /* at or below this, the level blinks */
#define GAUGE_BLINK_MS  180

/*
 * Unloaded CR2477 voltage -> charge. Lithium coin cells sit near 3.0 V for
 * most of their life and fall off at the end, so this is only a rough
 * guide. VDD is measured with the internal reference (+-3..5 %).
 */
static const struct { uint16_t mv; uint8_t pct; } CURVE[] = {
    {3150, 100}, {3050, 80}, {2950, 55}, {2850, 30}, {2750, 15}, {2600, 0},
};
/* ---------------------------------------------------------------------- */

#define CURVE_N (sizeof(CURVE) / sizeof(CURVE[0]))

static uint8_t pct;
static uint32_t t_ms;

uint8_t gauge_percent(uint32_t mv)
{
    if (mv >= CURVE[0].mv) return CURVE[0].pct;
    for (unsigned k = 1; k < CURVE_N; k++) {
        if (mv >= CURVE[k].mv) {
            uint32_t span = CURVE[k - 1].mv - CURVE[k].mv;
            uint32_t into = mv - CURVE[k].mv;
            return (uint8_t)(CURVE[k].pct + (CURVE[k - 1].pct - CURVE[k].pct) * into / span);
        }
    }
    return 0;
}

void gauge_start(uint8_t p)
{
    pct = p > 100 ? 100 : p;
    t_ms = 0;
}

uint32_t gauge_update(uint32_t frame_ms, uint8_t out[RING_LEDS])
{
    t_ms += frame_ms;

    uint32_t rise = t_ms >= GAUGE_RISE_MS ? 65535U : smooth16((uint16_t)(t_ms * 65535U / GAUGE_RISE_MS));
    /* 100 % fills both halves of the ring up to the top. */
    uint32_t half = (uint32_t)(((uint64_t)(RING_POS / 2) * pct / 100U * rise) >> 16);

    int lit = 1;
    if (pct <= GAUGE_LOW_PCT && t_ms >= GAUGE_RISE_MS && t_ms < GAUGE_RISE_MS + GAUGE_HOLD_MS) {
        lit = ((t_ms - GAUGE_RISE_MS) / GAUGE_BLINK_MS) % 2U == 0U;
    }

    fx_buf_t buf;
    fx_clear(buf);
    for (int i = 0; i < RING_LEDS; i++) fx_max(buf, i, GAUGE_SCALE_LVL);
    if (lit && half > 0) {
        fx_arc(buf, RING_POS_BOTTOM, half, GAUGE_LVL);
    }
    fx_out(buf, out);

    uint32_t fade_at = GAUGE_RISE_MS + GAUGE_HOLD_MS;
    if (t_ms < fade_at) return 0;
    if (t_ms >= fade_at + GAUGE_FADE_MS) return 256;
    return (t_ms - fade_at) * 256U / GAUGE_FADE_MS;
}
