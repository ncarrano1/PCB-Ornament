/*
 * ornament_core.c - mode switching and crossfade. Pure C.
 */
#include "ornament_core.h"
#include "mode.h"
#include "fx.h"
#include "gauge.h"

static uint32_t rng;
static uint8_t cur;       /* mode being shown (or faded to) */
static uint8_t prev;      /* mode being faded out */
static uint32_t fade_ms;  /* time into the crossfade; >= CORE_FADE_MS when done */
static int gauge_on;      /* battery gauge showing before the mode */

void core_init(uint32_t seed, uint8_t mode)
{
    rng = xs_seed(seed);
    cur = mode < MODE_COUNT ? mode : 0;
    prev = cur;
    fade_ms = CORE_FADE_MS;
    gauge_on = 0;
    MODES[cur]->init(xs32(&rng));
}

void core_show_gauge(uint8_t pct)
{
    gauge_start(pct);
    gauge_on = 1;
}

void core_stir(uint32_t entropy)
{
    rng = xs_seed(rng ^ (entropy * 0x9E3779B9U));
    xs32(&rng);
}

void core_set_mode(uint8_t mode)
{
    if (mode >= MODE_COUNT || mode == cur) {
        return;
    }
    /* A switch during a fade drops the oldest mode (short presses in a row). */
    prev = cur;
    cur = mode;
    fade_ms = 0;
    MODES[cur]->init(xs32(&rng));
}

void core_next_mode(void)
{
    core_set_mode((uint8_t)((cur + 1U) % MODE_COUNT));
}

uint8_t core_mode(void)
{
    return cur;
}

void core_frame(uint8_t out[RING_LEDS])
{
    if (gauge_on) {
        uint8_t g[RING_LEDS];
        uint32_t a = gauge_update(CORE_FRAME_MS, g);
        if (a == 0) {
            for (int i = 0; i < RING_LEDS; i++) out[i] = g[i];
            return;
        }
        /* Mode fades in over the gauge. */
        MODES[cur]->update(CORE_FRAME_MS, out);
        if (a >= 256U) {
            gauge_on = 0;
            return;
        }
        for (int i = 0; i < RING_LEDS; i++) {
            out[i] = (uint8_t)((g[i] * (256U - a) + out[i] * a + 128U) >> 8);
        }
        return;
    }

    MODES[cur]->update(CORE_FRAME_MS, out);
    if (fade_ms >= CORE_FADE_MS) {
        return;
    }

    uint8_t old[RING_LEDS];
    MODES[prev]->update(CORE_FRAME_MS, old);
    fade_ms += CORE_FRAME_MS;
    uint32_t a = fade_ms >= CORE_FADE_MS ? 256U : (fade_ms * 256U) / CORE_FADE_MS;
    /* Smooth the fade's start and end. */
    a = smooth16((uint16_t)(a >= 256U ? 65535U : a << 8)) >> 8;
    for (int i = 0; i < RING_LEDS; i++) {
        out[i] = (uint8_t)((old[i] * (256U - a) + out[i] * a + 128U) >> 8);
    }
}
