/*
 * mode_hourglass.c - 14. Hourglass: sand fills the top of the ring and
 * trickles grain by grain down both sides into a growing pile at the
 * bottom. When the top is empty, the whole picture turns over (rotates half
 * a turn) and it starts again.
 */
#include "mode.h"
#include "fx.h"

/* ---- Tweakable ------------------------------------------------------- */
#define HG_RUN_MS      30000  /* time for all the sand to fall */
#define HG_SAND_LEDS   6      /* sand reaches this many LEDs each side of centre */
#define HG_SAND_LVL    140
#define HG_GRAIN_MS    150    /* a grain leaves the top this often */
#define HG_GRAIN_PEAK  210
#define HG_G_Q12       51     /* gravity, ring units/ms^2 * 4096 */
#define HG_GRAIN_V0    2      /* speed leaving the sand, ring units/ms (~8 LEDs/s) */
#define HG_HOLD_MS     900    /* pause when empty, before turning over */
#define HG_FLIP_MS     1100   /* time to turn over */
#define HG_MAX_GRAINS  12
/* ---------------------------------------------------------------------- */

#define SAND ((uint32_t)HG_SAND_LEDS * RING_POS_PER_LED)

typedef enum { HG_RUN, HG_HOLD, HG_FLIP } hg_state_t;

typedef struct {
    uint8_t on;
    int8_t side;
    int32_t s_q12;  /* distance from the top, ring units * 4096 */
    int32_t v_q12;
} grain_t;

static hg_state_t state;
static uint32_t t_ms;
static int32_t grain_ms;
static int8_t next_side;
static int8_t flip_dir;
static grain_t gr[HG_MAX_GRAINS];

static void init(uint32_t seed)
{
    (void)seed;
    state = HG_RUN;
    t_ms = 0;
    grain_ms = 0;
    next_side = 1;
    flip_dir = 1;
    for (int i = 0; i < HG_MAX_GRAINS; i++) gr[i].on = 0;
}

static void update(uint32_t frame_ms, uint8_t out[RING_LEDS])
{
    fx_buf_t buf;
    fx_clear(buf);
    t_ms += frame_ms;

    if (state == HG_FLIP) {
        /* Turn the full bottom pile over to the top. */
        uint32_t a = t_ms >= HG_FLIP_MS ? 65535U : smooth16((uint16_t)(t_ms * 65535U / HG_FLIP_MS));
        int32_t rot = (int32_t)((RING_POS_BOTTOM * a) >> 16) * flip_dir;
        fx_arc(buf, ring_wrap(RING_POS_BOTTOM + rot), SAND, HG_SAND_LVL);
        if (t_ms >= HG_FLIP_MS) {
            state = HG_RUN;
            t_ms = 0;
            flip_dir = (int8_t)-flip_dir;
        }
        fx_out(buf, out);
        return;
    }

    uint32_t frac = state == HG_RUN ? (t_ms >= HG_RUN_MS ? 65536U : (t_ms << 16) / HG_RUN_MS) : 65536U;
    uint32_t upper = (uint32_t)(((uint64_t)SAND * (65536U - frac)) >> 16);
    uint32_t lower = SAND - upper;

    if (state == HG_RUN) {
        grain_ms -= (int32_t)frame_ms;
        if (grain_ms <= 0 && upper > 0) {
            grain_ms = HG_GRAIN_MS;
            for (int i = 0; i < HG_MAX_GRAINS; i++) {
                if (!gr[i].on) {
                    gr[i].on = 1;
                    gr[i].side = next_side;
                    gr[i].s_q12 = (int32_t)upper * 4096;
                    gr[i].v_q12 = HG_GRAIN_V0 * 4096;
                    next_side = (int8_t)-next_side;
                    break;
                }
            }
        }
        if (t_ms >= HG_RUN_MS) {
            state = HG_HOLD;
            t_ms = 0;
        }
    } else if (state == HG_HOLD && t_ms >= HG_HOLD_MS) {
        state = HG_FLIP;
        t_ms = 0;
    }

    if (upper > 0) fx_arc(buf, RING_POS_TOP, upper, HG_SAND_LVL);
    if (lower > 0) fx_arc(buf, RING_POS_BOTTOM, lower, HG_SAND_LVL);

    for (int i = 0; i < HG_MAX_GRAINS; i++) {
        grain_t *g = &gr[i];
        if (!g->on) continue;
        uint32_t s = (uint32_t)g->s_q12 >> 12;
        g->v_q12 += fx_gravity(s, HG_G_Q12) * (int32_t)frame_ms;
        g->s_q12 += g->v_q12 * (int32_t)frame_ms;
        s = (uint32_t)g->s_q12 >> 12;
        if (s + lower >= RING_POS_BOTTOM) {
            g->on = 0;
            continue;
        }
        fx_point(buf, g->side > 0 ? s : ring_wrap(-(int32_t)s), HG_GRAIN_PEAK);
    }
    fx_out(buf, out);
}

const mode_def_t mode_hourglass = {"hourglass", init, update};
