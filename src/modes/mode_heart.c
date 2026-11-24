/*
 * mode_heart.c - 7. Heartbeat: a "lub-dub" double pulse starting at the
 * top and rippling down both sides to the bottom, then a pause. 60 bpm.
 */
#include "mode.h"
#include "fx.h"

/* ---- Tweakable ------------------------------------------------------- */
#define HEART_BEAT_MS   1000  /* 60 bpm */
#define HEART_DUB_MS    230   /* "dub" after "lub" */
#define HEART_LUB       255   /* pulse strengths */
#define HEART_DUB       185
#define HEART_TRAVEL_MS 300   /* top-to-bottom ripple time */
#define HEART_ATTACK_MS 35
#define HEART_DECAY_MS  110
#define HEART_FADE_PCT  70    /* brightness left at the bottom, % */
/* ---------------------------------------------------------------------- */

static uint32_t t_ms;

static void init(uint32_t seed)
{
    (void)seed;
    t_ms = 0;
}

static void update(uint32_t frame_ms, uint8_t out[RING_LEDS])
{
    t_ms = (t_ms + frame_ms) % HEART_BEAT_MS;

    for (int i = 0; i < RING_LEDS; i++) {
        /* Distance down the ring from the top (same on both sides). */
        uint32_t d = angle_from_top(i);
        if (d > RING_POS / 2) d = RING_POS - d;
        int32_t delay = (int32_t)(d * HEART_TRAVEL_MS / (RING_POS / 2));
        uint32_t fade = 100U - (100U - HEART_FADE_PCT) * d / (RING_POS / 2);

        uint32_t v = 0;
        /* This beat and the tail of the previous one. */
        for (int32_t back = 0; back <= HEART_BEAT_MS; back += HEART_BEAT_MS) {
            int32_t t = (int32_t)t_ms + back - delay;
            v += (HEART_LUB * (uint32_t)fx_envelope(t, HEART_ATTACK_MS, HEART_DECAY_MS)) >> 16;
            v += (HEART_DUB * (uint32_t)fx_envelope(t - HEART_DUB_MS, HEART_ATTACK_MS,
                                                    HEART_DECAY_MS)) >> 16;
        }
        v = v * fade / 100U;
        out[i] = (uint8_t)(v > 255U ? 255U : v);
    }
}

const mode_def_t mode_heart = {"heartbeat", init, update};
