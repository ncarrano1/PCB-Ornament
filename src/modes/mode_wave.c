/*
 * mode_wave.c - 1. Traveling wave: a smooth raised-cosine bump circling
 * clockwise. Integer port of the verified `wave` program (gamma now comes
 * from the engine, giving the same curve).
 */
#include "mode.h"
#include "fx.h"

/* ---- Tweakable ------------------------------------------------------- */
#define WAVE_WIDTH_LEDS 8     /* full width of the bump */
#define WAVE_PERIOD_MS  4000  /* one turn of the ring */
#define WAVE_PEAK       255   /* 0..255 */
/* ---------------------------------------------------------------------- */

static uint32_t phase; /* 2^32 = one turn */

static void init(uint32_t seed)
{
    (void)seed;
    phase = 0;
}

static void update(uint32_t frame_ms, uint8_t out[RING_LEDS])
{
    phase += (uint32_t)(4294967296ULL / WAVE_PERIOD_MS) * frame_ms;
    uint32_t pos = (uint32_t)(((uint64_t)phase * RING_POS) >> 32);

    fx_buf_t buf;
    fx_clear(buf);
    fx_blob(buf, pos, WAVE_WIDTH_LEDS * RING_POS_PER_LED / 2, WAVE_PEAK);
    fx_out(buf, out);
}

const mode_def_t mode_wave = {"wave", init, update};
