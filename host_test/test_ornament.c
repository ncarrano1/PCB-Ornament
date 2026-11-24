/*
 * test_ornament.c - host test for the ornament modes and banked engine.
 * Run with:  make -C host_test
 *
 * Every mode runs for 60 simulated seconds through the real frame builder:
 *   - every duty code is within 0..RING_CAP_CODE
 *   - no time slice has more than 20 LEDs high, and all are in one bank
 *   - GPIO words are well-formed and never touch SWD/button/PC13-15 pins
 *   - output is deterministic for a fixed seed
 * plus crossfades between every pair of modes and the geometry helpers.
 */
#include <stdio.h>
#include <string.h>
#include "ring_frame.h"
#include "ring_geom.h"
#include "fx.h"
#include "mode.h"
#include "ornament_core.h"
#include "button_fsm.h"

static int failures;
#define CHECK(cond, ...)                                                   \
    do {                                                                   \
        if (!(cond)) {                                                     \
            if (++failures <= 25) {                                        \
                printf("FAIL %s:%d: ", __FILE__, __LINE__);                \
                printf(__VA_ARGS__);                                       \
                printf("\n");                                              \
            }                                                              \
        }                                                                  \
    } while (0)

#define SIM_MS     60000
#define SIM_FRAMES (SIM_MS / CORE_FRAME_MS)

static const uint32_t FORBIDDEN[RING_PORTS] = {
    (1u << 13) | (1u << 14),              /* PA13/PA14: SWD */
    (1u << 15) | (1u << 13) | (1u << 14), /* PB15 button, PB13/14 unused */
    (1u << 13) | (1u << 14) | (1u << 15), /* PC13-15 */
};

static uint32_t bank_mask[RING_PORTS][RING_BANKS];

/* Check one built frame. Returns the max LEDs high in any slice. */
static int check_frame(const uint8_t code[RING_LEDS], const ring_frame_t *f,
                       const char *ctx, int frame)
{
    int worst = 0;
    for (int i = 0; i < RING_LEDS; i++) {
        CHECK(code[i] <= RING_CAP_CODE, "%s frame %d LED %d: code %u > cap %d",
              ctx, frame, i, code[i], RING_CAP_CODE);
    }
    for (int s = 0; s < RING_SLICES; s++) {
        int on = 0, bank = ring_slice_bank(s);
        for (int p = 0; p < RING_PORTS; p++) {
            uint32_t set = f->slice[s].set[p], clr = f->slice[s].clr[p];
            on += __builtin_popcount(set);
            CHECK((set & ~bank_mask[p][bank]) == 0,
                  "%s frame %d slice %d: pin outside bank %d", ctx, frame, s, bank);
            CHECK((set & clr) == 0, "%s slice %d: set/clr overlap", ctx, s);
            CHECK((set | clr) == ring_port_mask(p), "%s slice %d: set|clr != LED mask", ctx, s);
            CHECK(((set | clr) & FORBIDDEN[p]) == 0, "%s slice %d: forbidden pin", ctx, s);
        }
        CHECK(on <= RING_MAX_ON, "%s frame %d slice %d: %d LEDs on", ctx, frame, s, on);
        if (on > worst) worst = on;
    }
    return worst;
}

static uint32_t fnv(uint32_t h, const uint8_t *d, int n)
{
    for (int i = 0; i < n; i++) {
        h = (h ^ d[i]) * 16777619u;
    }
    return h;
}

/* Run one mode alone for SIM_MS, checking every frame. Returns output hash. */
static uint32_t run_mode(int m, uint32_t seed, int *worst_out, int *lit_out)
{
    uint8_t out[RING_LEDS], code[RING_LEDS];
    ring_frame_t f;
    uint32_t h = 2166136261u;
    int worst = 0, lit_frames = 0;

    core_init(seed, (uint8_t)m);
    for (int n = 0; n < SIM_FRAMES; n++) {
        core_frame(out);
        ring_codes(out, code);
        ring_build(code, &f);
        int w = check_frame(code, &f, MODES[m]->name, n);
        if (w > worst) worst = w;
        int any = 0;
        for (int i = 0; i < RING_LEDS; i++) any |= code[i];
        lit_frames += any != 0;
        h = fnv(h, out, RING_LEDS);
    }
    if (worst_out) *worst_out = worst;
    if (lit_out) *lit_out = lit_frames;
    return h;
}

static void test_modes(void)
{
    for (int m = 0; m < MODE_COUNT; m++) {
        int worst, lit;
        uint32_t h1 = run_mode(m, 1234, &worst, &lit);
        uint32_t h2 = run_mode(m, 1234, NULL, NULL);
        CHECK(h1 == h2, "%s: not deterministic for a fixed seed", MODES[m]->name);
        CHECK(lit > SIM_FRAMES / 2, "%s: dark in %d of %d frames", MODES[m]->name,
              SIM_FRAMES - lit, SIM_FRAMES);
        uint32_t h3 = run_mode(m, 98765, NULL, NULL);
        printf("  %2d %-13s max %2d LEDs/slice, lit %3d%% of frames, seed-sensitive: %s\n",
               m + 1, MODES[m]->name, worst, lit * 100 / SIM_FRAMES,
               h3 != h1 ? "yes" : "no");
    }
}

static void test_crossfades(void)
{
    uint8_t out[RING_LEDS], code[RING_LEDS];
    ring_frame_t f;
    for (int a = 0; a < MODE_COUNT; a++) {
        for (int b = 0; b < MODE_COUNT; b++) {
            if (a == b) continue;
            core_init(7, (uint8_t)a);
            for (int n = 0; n < 100; n++) core_frame(out);
            core_set_mode((uint8_t)b);
            CHECK(core_mode() == b, "switch %d->%d failed", a, b);
            for (int n = 0; n < (CORE_FADE_MS / CORE_FRAME_MS) + 5; n++) {
                core_frame(out);
                ring_codes(out, code);
                ring_build(code, &f);
                check_frame(code, &f, "crossfade", n);
            }
        }
    }
    core_init(7, (uint8_t)(MODE_COUNT - 1));
    core_next_mode();
    CHECK(core_mode() == 0, "next mode does not wrap to mode 1");
    printf("  crossfades between all %d x %d mode pairs OK, wrap %d -> 1 OK\n",
           MODE_COUNT, MODE_COUNT - 1, MODE_COUNT);
}

static void test_engine_limits(void)
{
    /* Full ring at full brightness: banking keeps every slice at 20. */
    uint8_t out[RING_LEDS], code[RING_LEDS];
    ring_frame_t f;
    memset(out, 255, sizeof(out));
    ring_codes(out, code);
    ring_build(code, &f);
    int w = check_frame(code, &f, "all-on", 0);
    CHECK(w == RING_MAX_ON, "all-on: max %d LEDs per slice, expected exactly 20", w);
    CHECK(code[0] == RING_CAP_CODE, "all-on: code %u, expected full cap %d", code[0], RING_CAP_CODE);

    /* Random frames. */
    uint32_t s = 42;
    for (int n = 0; n < 20000; n++) {
        for (int i = 0; i < RING_LEDS; i++) out[i] = (uint8_t)xs32(&s);
        ring_codes(out, code);
        ring_build(code, &f);
        check_frame(code, &f, "random", n);
    }
    printf("  engine: all-on runs at full cap (code %d/%d, %d%% duty) with 20 LEDs per slice\n",
           RING_CAP_CODE, RING_CODE_MAX, BRIGHTNESS_CAP_PCT);
}

static void test_geometry(void)
{
    CHECK(angle_from_top(39) == 0, "D40 is not at the top");
    CHECK(angle_from_top(19) == RING_POS / 2, "D20 is not at the bottom");
    CHECK(height(39) > 32700 && height(19) < -32700, "height top/bottom wrong");
    CHECK(height(9) > -300 && height(9) < 300, "D10 (3 o'clock) not at mid height");
    CHECK(mirror(39) == 39 && mirror(19) == 19 && mirror(0) == 38 && mirror(38) == 0,
          "mirror endpoints wrong");
    for (int i = 0; i < RING_LEDS; i++) {
        CHECK(mirror(mirror(i)) == i, "mirror not an involution at %d", i);
        CHECK(height(i) == height(mirror(i)), "mirror(%d) not at same height", i);
        CHECK(ring_led_at((int32_t)(ring_led_pos(i) / RING_POS_PER_LED)) == i,
              "ring_led_at/ring_led_pos mismatch at %d", i);
    }
    /* Right side (0..18) descends: height strictly decreasing. */
    for (int i = 0; i < 19; i++) {
        CHECK(height(i) > height(i + 1), "right side not descending at %d", i);
    }
    printf("  geometry: angle_from_top, height, mirror OK\n");
}

static void test_button(void)
{
    btn_t b;
    uint32_t t = 0;
    int shorts = 0, longs = 0;
    btn_init(&b, false, t);
    /* short press with contact bounce */
    for (int k = 0; k < 6; k++) btn_update(&b, k & 1, t++);
    for (; t < 200; t++) shorts += btn_update(&b, true, t) == BTN_SHORT;
    for (; t < 300; t++) shorts += btn_update(&b, false, t) == BTN_SHORT;
    CHECK(shorts == 1, "short press: %d events", shorts);
    /* long press: LONG once while held, nothing on release */
    shorts = 0;
    for (; t < 1500; t++) {
        btn_event_t e = btn_update(&b, true, t);
        longs += e == BTN_LONG;
        shorts += e == BTN_SHORT;
    }
    for (; t < 1600; t++) shorts += btn_update(&b, false, t) == BTN_SHORT;
    CHECK(longs == 1 && shorts == 0, "long press: %d long, %d short", longs, shorts);
    /* wake press: already down at init -> ignored entirely */
    btn_init(&b, true, t);
    longs = shorts = 0;
    for (int k = 0; k < 2000; k++, t++) {
        btn_event_t e = btn_update(&b, k < 1500, t);
        longs += e == BTN_LONG;
        shorts += e == BTN_SHORT;
    }
    CHECK(longs == 0 && shorts == 0, "wake press not ignored: %d long, %d short", longs, shorts);
    printf("  button: debounce, short, long, and ignored wake press OK\n");
}

int main(void)
{
    for (int p = 0; p < RING_PORTS; p++) {
        for (int k = 0; k < RING_BANKS; k++) bank_mask[p][k] = ring_port_bank_mask(p, k);
    }
    test_geometry();
    test_engine_limits();
    test_modes();
    test_crossfades();
    test_button();
    if (failures) {
        printf("FAILED: %d check(s)\n", failures);
        return 1;
    }
    printf("PASSED\n");
    return 0;
}
