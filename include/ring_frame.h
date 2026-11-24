/*
 * ring_frame.h - banked BCM brightness layer for the ornament. Pure C.
 *
 * The 40 LEDs are split into two banks of 20: bank A = even indices,
 * bank B = odd indices. Every BCM time slice drives pins from ONE bank
 * only, so no more than 20 LEDs (LED_MAX_ON) can ever be high at once,
 * whatever the pattern (CLAUDE.md rule 1, enforced structurally).
 *
 * Each bank gets 7 bit-planes; slices are interleaved A6 B6 A5 B5 ... A0 B0.
 * An LED is therefore lit at most 50 % of the time. Gamma correction and the
 * global brightness cap are applied here, not in the modes.
 */
#ifndef RING_FRAME_H
#define RING_FRAME_H

#include <stdint.h>

#define RING_LEDS     40
#define RING_BANKS    2
#define RING_BITS     7                         /* per bank */
#define RING_SLICES   (RING_BANKS * RING_BITS)  /* 14 */
#define RING_PORTS    3                         /* GPIOA, GPIOB, GPIOC */
#define RING_MAX_ON   20                        /* == LED_MAX_ON */
#define RING_CODE_MAX ((1 << RING_BITS) - 1)    /* 127 = 50 % duty */

/*
 * Global brightness cap, percent of full LED duty (max 50: a bank is only
 * on half the time). 30 % keeps the coin cell happy.
 */
#define BRIGHTNESS_CAP_PCT 30
#define RING_CAP_CODE ((RING_CODE_MAX * 2 * BRIGHTNESS_CAP_PCT + 50) / 100)

_Static_assert(BRIGHTNESS_CAP_PCT > 0 && BRIGHTNESS_CAP_PCT <= 50,
               "cap must be 1..50 % (banked: each LED is on <= 50 %)");

/* Even/odd banks hold exactly RING_MAX_ON LEDs each. */
_Static_assert(RING_LEDS == RING_BANKS * RING_MAX_ON, "banks must hold 20 LEDs");

enum { RING_PORT_A = 0, RING_PORT_B = 1, RING_PORT_C = 2 };

/* One slice: pins to drive low (BRR) and high (BSRR) on each port. */
typedef struct {
    uint32_t clr[RING_PORTS];
    uint32_t set[RING_PORTS];
} ring_slice_t;

typedef struct {
    ring_slice_t slice[RING_SLICES];
} ring_frame_t;

/* Bank (0 = A, 1 = B) and bit-plane shown during slice s. */
static inline int ring_slice_bank(int s) { return s & 1; }
static inline int ring_slice_bit(int s) { return RING_BITS - 1 - (s >> 1); }

/* Bank of LED i. */
static inline int ring_led_bank(int i) { return i & 1; }

/* Perceptual brightness 0..255 -> duty codes 0..RING_CAP_CODE (gamma + cap). */
void ring_codes(const uint8_t bright[RING_LEDS], uint8_t code[RING_LEDS]);

/* Duty codes -> per-slice GPIO words. */
void ring_build(const uint8_t code[RING_LEDS], ring_frame_t *out);

/* LED pin mask on a port, all LEDs or one bank only. */
uint32_t ring_port_mask(int port);
uint32_t ring_port_bank_mask(int port, int bank);

#endif
