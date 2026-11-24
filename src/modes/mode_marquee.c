/*
 * mode_marquee.c - 19. Marquee chase: the classic theatre-sign Christmas
 * chase. Every 4th LED is lit and the pattern steps along with a soft
 * crossfade; now and then it pauses and reverses, or races for a moment.
 */
#include "mode.h"
#include "fx.h"

/* ---- Tweakable ------------------------------------------------------- */
#define MQ_SPACING      4     /* lit every Nth LED (must divide 40) */
#define MQ_STEP_MS      140   /* time per step */
#define MQ_FADE_MS      50    /* crossfade within a step */
#define MQ_LVL          230
#define MQ_EVENT_MIN_MS 6000  /* time between reversals / bursts */
#define MQ_EVENT_MAX_MS 14000
#define MQ_PAUSE_MS     500   /* pause before reversing */
#define MQ_BURST_PCT    30    /* chance an event is a speed burst instead */
#define MQ_BURST_MS     1500
#define MQ_BURST_DIV    3     /* burst steps this many times faster */
/* ---------------------------------------------------------------------- */

_Static_assert(RING_LEDS % MQ_SPACING == 0, "MQ_SPACING must divide 40");

static uint32_t from_pos, to_pos;  /* ring units of bulb 0 */
static uint32_t t_ms;
static int8_t dir;
static int32_t event_ms, pause_ms, burst_ms;
static uint32_t rng;

static void init(uint32_t seed)
{
    rng = xs_seed(seed);
    from_pos = to_pos = 0;
    t_ms = 0;
    dir = 1;
    event_ms = (int32_t)xs_range(&rng, MQ_EVENT_MIN_MS, MQ_EVENT_MAX_MS);
    pause_ms = burst_ms = 0;
}

static void update(uint32_t frame_ms, uint8_t out[RING_LEDS])
{
    const int32_t dt = (int32_t)frame_ms;
    event_ms -= dt;
    if (event_ms <= 0) {
        event_ms = (int32_t)xs_range(&rng, MQ_EVENT_MIN_MS, MQ_EVENT_MAX_MS);
        if (xs_range(&rng, 0, 99) < MQ_BURST_PCT) {
            burst_ms = MQ_BURST_MS;
        } else {
            pause_ms = MQ_PAUSE_MS;
            dir = (int8_t)-dir;
        }
    }
    if (burst_ms > 0) burst_ms -= dt;

    uint32_t step_ms = burst_ms > 0 ? MQ_STEP_MS / MQ_BURST_DIV : MQ_STEP_MS;
    uint32_t fade_ms = burst_ms > 0 ? MQ_FADE_MS / MQ_BURST_DIV : MQ_FADE_MS;
    if (pause_ms > 0) {
        pause_ms -= dt;
    } else {
        t_ms += frame_ms;
        if (t_ms >= step_ms) {
            t_ms -= step_ms;
            from_pos = to_pos;
            to_pos = ring_wrap((int32_t)to_pos + dir * RING_POS_PER_LED);
        }
    }

    /* Glide from the old to the new position during the fade. */
    uint32_t a = t_ms >= fade_ms ? 65535U : smooth16((uint16_t)(t_ms * 65535U / fade_ms));
    int32_t delta = (int32_t)to_pos - (int32_t)from_pos;
    if (delta > RING_POS / 2) delta -= RING_POS;
    if (delta < -RING_POS / 2) delta += RING_POS;
    uint32_t base = ring_wrap((int32_t)from_pos + (int32_t)(((int64_t)delta * a) >> 16));

    fx_buf_t buf;
    fx_clear(buf);
    for (int n = 0; n < RING_LEDS / MQ_SPACING; n++) {
        fx_point(buf, base + (uint32_t)n * MQ_SPACING * RING_POS_PER_LED, MQ_LVL);
    }
    fx_out(buf, out);
}

const mode_def_t mode_marquee = {"marquee", init, update};
