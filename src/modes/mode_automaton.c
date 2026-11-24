/*
 * mode_automaton.c - 18. Cellular automaton: an elementary 1-D cellular
 * automaton on the 40 LEDs (wrapping around), one generation every quarter
 * second with a soft crossfade. It cycles through rules 30 (chaos), 90
 * (Sierpinski, grown from a single cell), 110 and 150, and reseeds itself
 * if the pattern dies out or freezes.
 */
#include "mode.h"
#include "fx.h"

/* ---- Tweakable ------------------------------------------------------- */
#define CA_GEN_MS       260   /* time per generation */
#define CA_FADE_MS      120   /* crossfade between generations */
#define CA_GENS_PER_RULE 60   /* generations before moving to the next rule */
#define CA_LVL          190
#define CA_SEED_PCT     35    /* live cells in a random seed */
/* ---------------------------------------------------------------------- */

static const uint8_t RULES[] = {30, 90, 110, 150};
#define RULE_COUNT (sizeof(RULES) / sizeof(RULES[0]))

static uint64_t prev, cur, older;
static uint32_t t_ms;
static uint32_t gens;
static uint8_t rule_idx;
static uint32_t rng;

#define ALL ((1ULL << RING_LEDS) - 1ULL)

static void seed_cells(void)
{
    older = prev = 0;
    if (RULES[rule_idx] == 90) {
        cur = 1ULL << RING_LED_TOP;  /* single cell: Sierpinski triangle */
        return;
    }
    cur = 0;
    for (int i = 0; i < RING_LEDS; i++) {
        if (xs_range(&rng, 0, 99) < CA_SEED_PCT) cur |= 1ULL << i;
    }
    if (cur == 0) cur = 1;
}

static uint64_t step(uint64_t c, uint8_t rule)
{
    uint64_t n = 0;
    for (int i = 0; i < RING_LEDS; i++) {
        int l = (int)((c >> ((i + RING_LEDS - 1) % RING_LEDS)) & 1U);
        int m = (int)((c >> i) & 1U);
        int r = (int)((c >> ((i + 1) % RING_LEDS)) & 1U);
        if ((rule >> (l << 2 | m << 1 | r)) & 1U) n |= 1ULL << i;
    }
    return n;
}

static void init(uint32_t seed)
{
    rng = xs_seed(seed);
    rule_idx = 0;
    gens = 0;
    t_ms = 0;
    seed_cells();
}

static void update(uint32_t frame_ms, uint8_t out[RING_LEDS])
{
    t_ms += frame_ms;
    if (t_ms >= CA_GEN_MS) {
        t_ms -= CA_GEN_MS;
        older = prev;
        prev = cur;
        if (++gens >= CA_GENS_PER_RULE) {
            gens = 0;
            rule_idx = (uint8_t)((rule_idx + 1U) % RULE_COUNT);
            seed_cells();
            prev = 0; /* fade the new seed in from dark */
        } else {
            cur = step(cur, RULES[rule_idx]) & ALL;
            /* died, frozen, or blinking between two states: reseed */
            if (cur == 0 || cur == prev || cur == older) {
                uint64_t keep = prev;
                seed_cells();
                prev = keep;
            }
        }
    }

    uint32_t a = t_ms >= CA_FADE_MS ? 65535U : smooth16((uint16_t)(t_ms * 65535U / CA_FADE_MS));
    for (int i = 0; i < RING_LEDS; i++) {
        uint32_t was = ((prev >> i) & 1U) ? 65535U : 0U;
        uint32_t now = ((cur >> i) & 1U) ? 65535U : 0U;
        uint32_t v = (uint32_t)(((uint64_t)was * (65535U - a) + (uint64_t)now * a) >> 16);
        out[i] = (uint8_t)((CA_LVL * v) >> 16);
    }
}

const mode_def_t mode_automaton = {"automaton", init, update};
