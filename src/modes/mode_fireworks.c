/*
 * mode_fireworks.c - 10. Fireworks: a spark shoots up from the bottom along
 * one side, bursts at the top into particles that fall down both sides
 * under gravity and fade, then it repeats from a random side.
 */
#include "mode.h"
#include "fx.h"

/* ---- Tweakable ------------------------------------------------------- */
#define FW_LAUNCH_MS     850   /* bottom to top */
#define FW_TRAIL_SAMPLES 4     /* rocket trail copies */
#define FW_TRAIL_STEP_MS 35
#define FW_ROCKET        200   /* rocket brightness */
#define FW_FLASH_MS      160   /* burst flash fade */
#define FW_FLASH_RADIUS  5     /* LEDs */
#define FW_PARTICLES_MIN 6
#define FW_PARTICLES_MAX 10
#define FW_V0_MIN        2     /* particle launch speed, ring units/ms */
#define FW_V0_MAX        9
#define FW_FADE_MIN_MS   800   /* particle fade time constant */
#define FW_FADE_MAX_MS   1500
#define FW_GRAVITY_Q12   51    /* tangential g, ring units/ms^2 * 4096 */
#define FW_DRAG_MS       1800  /* velocity damping time constant */
#define FW_SPARKLE_PCT   35    /* random per-frame flicker depth */
#define FW_PAUSE_MIN_MS  500
#define FW_PAUSE_MAX_MS  1500
#define FW_MAX_PARTICLES 10
#define FW_VISIBLE       34    /* dimmest perceptual level the engine still lights */
/* ---------------------------------------------------------------------- */

typedef enum { FW_PAUSE, FW_LAUNCH, FW_BURST } fw_state_t;

typedef struct {
    int32_t s_q12;     /* signed arc from the top, ring units * 4096 (+ = clockwise) */
    int32_t v_q12;     /* ring units/ms * 4096 */
    uint16_t age, fade;
    uint8_t peak;
} spark_t;

static fw_state_t state;
static int32_t timer;   /* ms in the current state, or until the next */
static int side;        /* +1 right side, -1 left */
static spark_t sp[FW_MAX_PARTICLES];
static int nsp;
static uint32_t rng;

static void init(uint32_t seed)
{
    rng = xs_seed(seed);
    state = FW_PAUSE;
    timer = 300;
    nsp = 0;
}

/* Rocket height: decelerating rise, s = BOTTOM * (1 - x)^2. */
static uint32_t rocket_arc(int32_t t)
{
    if (t < 0) t = 0;
    if (t > FW_LAUNCH_MS) t = FW_LAUNCH_MS;
    uint32_t r = (uint32_t)(FW_LAUNCH_MS - t) * 1024U / FW_LAUNCH_MS; /* 1-x, Q10 */
    return (uint32_t)RING_POS_BOTTOM * ((r * r) >> 10) >> 10; /* no 32-bit overflow */
}

static uint32_t arc_pos(uint32_t arc, int dir)
{
    return dir > 0 ? arc : ring_wrap(-(int32_t)arc);
}

static void burst(void)
{
    nsp = (int)xs_range(&rng, FW_PARTICLES_MIN, FW_PARTICLES_MAX);
    for (int k = 0; k < nsp; k++) {
        spark_t *p = &sp[k];
        int32_t v = (int32_t)xs_range(&rng, FW_V0_MIN * 4096U, FW_V0_MAX * 4096U);
        p->v_q12 = (k & 1) ? v : -v;  /* half each way */
        p->s_q12 = 0;
        p->age = 0;
        p->fade = (uint16_t)xs_range(&rng, FW_FADE_MIN_MS, FW_FADE_MAX_MS);
        p->peak = (uint8_t)xs_range(&rng, 170, 255);
    }
}

static void update(uint32_t frame_ms, uint8_t out[RING_LEDS])
{
    fx_buf_t buf;
    fx_clear(buf);
    timer += (int32_t)frame_ms;

    if (state == FW_PAUSE && timer >= 0) {
        state = FW_LAUNCH;
        timer = 0;
        side = (xs32(&rng) & 1U) ? 1 : -1;
    }

    if (state == FW_LAUNCH) {
        if (timer >= FW_LAUNCH_MS) {
            state = FW_BURST;
            timer = 0;
            burst();
        } else {
            /* Rocket with a fading trail (earlier positions, lower down). */
            uint32_t lvl = FW_ROCKET;
            for (int k = 0; k <= FW_TRAIL_SAMPLES; k++) {
                uint32_t pos = arc_pos(rocket_arc(timer - k * FW_TRAIL_STEP_MS), side);
                uint32_t sparkle = 100U - xs_range(&rng, 0, FW_SPARKLE_PCT) * (k > 0);
                fx_point(buf, pos, (uint8_t)(lvl * sparkle / 100U));
                lvl = lvl * 140U / 255U;
            }
        }
    }

    if (state == FW_BURST) {
        /* Flash at the top. */
        if (timer < FW_FLASH_MS * 4) {
            uint32_t e = exp_neg((uint32_t)timer * 256U / FW_FLASH_MS);
            uint32_t r = FW_FLASH_RADIUS * RING_POS_PER_LED * (65536U - e / 2U) >> 16;
            fx_blob(buf, RING_POS_TOP, r, (uint8_t)((255U * e) >> 16));
        }
        int alive = 0;
        for (int k = 0; k < nsp; k++) {
            spark_t *p = &sp[k];
            /* Gravity along the ring: g * sin(angle), always toward the bottom. */
            uint32_t pos = ring_wrap(p->s_q12 >> 12);
            int32_t a = (FW_GRAVITY_Q12 * sin16(ring_to_angle(pos))) >> 15;
            p->v_q12 += a * (int32_t)frame_ms;
            p->v_q12 -= p->v_q12 * (int32_t)frame_ms / FW_DRAG_MS;
            p->s_q12 += p->v_q12 * (int32_t)frame_ms;
            p->age = (uint16_t)(p->age + frame_ms);

            uint32_t b = (p->peak * (uint32_t)exp_neg((uint32_t)p->age * 256U / p->fade)) >> 16;
            if (b >= FW_VISIBLE) {
                alive = 1;
                uint32_t sparkle = 100U - xs_range(&rng, 0, FW_SPARKLE_PCT);
                fx_point(buf, ring_wrap(p->s_q12 >> 12), (uint8_t)(b * sparkle / 100U));
            }
        }
        if (!alive && timer > FW_FLASH_MS) {
            state = FW_PAUSE;
            timer = -(int32_t)xs_range(&rng, FW_PAUSE_MIN_MS, FW_PAUSE_MAX_MS);
        }
    }
    fx_out(buf, out);
}

const mode_def_t mode_fireworks = {"fireworks", init, update};
