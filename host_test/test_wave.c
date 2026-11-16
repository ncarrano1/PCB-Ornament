/*
 * test_wave.c - host-side test of the wave frame generator and the BCM
 * bit-plane builder. Run with:  make -C host_test
 *
 * Checks that no bit-plane ever has more than BCM_MAX_ON (20) LEDs on,
 * for every position of a full rotation in every mode.
 */
#include <stdio.h>
#include <stdlib.h>
#include "bcm_frame.h"
#include "wave_anim.h"

static int failures;

#define CHECK(cond, ...)                                              \
    do {                                                              \
        if (!(cond)) {                                                \
            failures++;                                               \
            if (failures <= 20) {                                     \
                printf("FAIL %s:%d: ", __FILE__, __LINE__);           \
                printf(__VA_ARGS__);                                  \
                printf("\n");                                         \
            }                                                         \
        }                                                             \
    } while (0)

static const char *MODE_NAMES[] = {"slow", "fast", "two", "off"};

/* Pins that must never appear in a GPIO word. */
static const uint32_t FORBIDDEN[BCM_PORTS] = {
    (1u << 13) | (1u << 14),              /* PA13/PA14: SWD */
    (1u << 15),                           /* PB15: button */
    (1u << 13) | (1u << 14) | (1u << 15), /* PC13-15 */
};

static int popcount(uint32_t x) { return __builtin_popcount(x); }

static int plane_on_count(const bcm_plane_t *pl)
{
    int n = 0;
    for (int p = 0; p < BCM_PORTS; p++) {
        n += popcount(pl->set[p]);
    }
    return n;
}

/* Structural checks plus the current limit. Returns max LEDs on in a plane. */
static int check_frame(const bcm_frame_t *f, const char *ctx, uint32_t pos)
{
    int max_on = 0;
    for (int b = 0; b < BCM_BITS; b++) {
        const bcm_plane_t *pl = &f->plane[b];
        int on = plane_on_count(pl);
        if (on > max_on) {
            max_on = on;
        }
        CHECK(on <= BCM_MAX_ON, "%s pos %u plane %d: %d LEDs on", ctx, pos, b, on);
        for (int p = 0; p < BCM_PORTS; p++) {
            uint32_t m = bcm_port_led_mask(p);
            CHECK((pl->set[p] & pl->clr[p]) == 0, "%s plane %d port %d: set/clr overlap", ctx, b, p);
            CHECK((pl->set[p] | pl->clr[p]) == m, "%s plane %d port %d: set|clr != LED mask", ctx, b, p);
            CHECK(((pl->set[p] | pl->clr[p]) & FORBIDDEN[p]) == 0, "%s plane %d port %d: touches forbidden pin", ctx, b, p);
            CHECK(((pl->set[p] | pl->clr[p]) >> 16) == 0, "%s plane %d port %d: bits above 15", ctx, b, p);
        }
    }
    return max_on;
}

static void test_pin_map(void)
{
    int total = 0;
    for (int p = 0; p < BCM_PORTS; p++) {
        total += popcount(bcm_port_led_mask(p));
        CHECK((bcm_port_led_mask(p) & FORBIDDEN[p]) == 0, "port %d map uses a forbidden pin", p);
    }
    CHECK(total == BCM_LED_COUNT, "pin map has %d distinct pins, want %d", total, BCM_LED_COUNT);
}

/* Every position of a full rotation, in every mode. */
static void test_modes_full_rotation(void)
{
    uint16_t duty[BCM_LED_COUNT];
    bcm_frame_t f;
    for (int m = 0; m < WAVE_MODE_COUNT; m++) {
        int worst = 0, limited = 0;
        for (uint32_t pos = 0; pos < WAVE_RING_POS; pos++) {
            wave_anim_render_at((wave_mode_t)m, pos, duty);
            uint8_t cap = bcm_build(duty, BCM_CAP_Q8, &f);
            if (cap != BCM_CAP_Q8) {
                limited++;
            }
            int on = check_frame(&f, MODE_NAMES[m], pos);
            if (on > worst) {
                worst = on;
            }
            if (m == WAVE_OFF) {
                CHECK(on == 0, "off mode lit %d LEDs at pos %u", on, pos);
            }
        }
        printf("  %-4s: %d positions, max %2d LEDs on in any plane, limiter used %d times\n",
               MODE_NAMES[m], WAVE_RING_POS, worst, limited);
    }
}

/* The limiter must bring impossible frames within budget. */
static void test_limiter_stress(void)
{
    uint16_t duty[BCM_LED_COUNT];
    bcm_frame_t f;
    const uint8_t caps[] = {BCM_CAP_Q8, 128, 255};

    for (unsigned c = 0; c < sizeof(caps); c++) {
        /* All 40 LEDs at full duty. */
        for (int i = 0; i < BCM_LED_COUNT; i++) duty[i] = 65535;
        uint8_t cap = bcm_build(duty, caps[c], &f);
        CHECK(cap < caps[c], "all-on at cap %u: limiter did not engage", caps[c]);
        check_frame(&f, "all-on", caps[c]);

        /* A wide plateau: 30 LEDs at the same duty share every bit. */
        for (int i = 0; i < BCM_LED_COUNT; i++) {
            duty[i] = (i >= 5 && i < 35) ? 50000 : 0;
        }
        cap = bcm_build(duty, caps[c], &f);
        CHECK(cap < caps[c], "wide plateau at cap %u: limiter did not engage", caps[c]);
        check_frame(&f, "wide", caps[c]);

        /* Pseudo-random frames. */
        uint32_t seed = 12345;
        for (int n = 0; n < 20000; n++) {
            for (int i = 0; i < BCM_LED_COUNT; i++) {
                seed = seed * 1103515245u + 12345u;
                duty[i] = (uint16_t)(seed >> 16);
            }
            bcm_build(duty, caps[c], &f);
            check_frame(&f, "random", (uint32_t)n);
        }
    }
    printf("  limiter: all-on, wide plateau and 60000 random frames within budget\n");
}

/* Seamless wrap D40 -> D1, and clockwise motion. */
static void test_wrap_and_direction(void)
{
    uint16_t a[BCM_LED_COUNT], b[BCM_LED_COUNT];
    for (int m = 0; m < WAVE_OFF; m++) {
        int max_step = 0, wrap_step = 0;
        for (uint32_t pos = 0; pos < WAVE_RING_POS; pos++) {
            uint32_t next = (pos + 1) % WAVE_RING_POS;
            wave_anim_render_at((wave_mode_t)m, pos, a);
            wave_anim_render_at((wave_mode_t)m, next, b);
            int step = 0;
            for (int i = 0; i < BCM_LED_COUNT; i++) {
                int d = abs((int)a[i] - (int)b[i]);
                if (d > step) step = d;
            }
            if (next == 0) wrap_step = step;
            else if (step > max_step) max_step = step;
        }
        CHECK(wrap_step > 0 && wrap_step <= max_step,
              "%s: wrap step %d vs max step %d", MODE_NAMES[m], wrap_step, max_step);
    }

    /* Peak sits on LED k at position k*256: clockwise D1 -> D40 -> D1. */
    for (int k = 0; k < BCM_LED_COUNT; k++) {
        wave_anim_render_at(WAVE_SLOW, (uint32_t)k * WAVE_POS_PER_LED, a);
        int best = 0;
        for (int i = 1; i < BCM_LED_COUNT; i++) {
            if (a[i] > a[best]) best = i;
        }
        CHECK(best == k, "peak at LED %d, want %d", best, k);
    }
    printf("  wrap D40->D1 is seamless, wave moves clockwise\n");
}

int main(void)
{
    wave_anim_init();
    test_pin_map();
    test_modes_full_rotation();
    test_limiter_stress();
    test_wrap_and_direction();

    if (failures) {
        printf("FAILED: %d check(s)\n", failures);
        return 1;
    }
    printf("PASSED\n");
    return 0;
}
