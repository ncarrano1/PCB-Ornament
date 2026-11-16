/*
 * bcm_frame.h - brightness layer for binary code modulation (BCM).
 *
 * Pure C (no HAL), so it also builds on the host for tests.
 * Turns per-LED duty values into precomputed per-bit-plane GPIO words,
 * applying the global brightness cap and the per-plane current limit.
 */
#ifndef BCM_FRAME_H
#define BCM_FRAME_H

#include <stdint.h>

#define BCM_LED_COUNT 40   /* must equal LED_COUNT (checked in led_bcm.c) */
#define BCM_BITS      8    /* 8-bit brightness, 8 bit-planes per frame */
#define BCM_PORTS     3    /* GPIOA, GPIOB, GPIOC, in that order */

/*
 * At most this many LEDs may be on in any bit-plane (CLAUDE.md rule 1).
 * Must equal LED_MAX_ON in board.h (checked in led_bcm.c).
 */
#define BCM_MAX_ON 20

/*
 * Global brightness cap, as a fraction of full duty in 1/256 units.
 * 77/256 is about 30 %. Raise it for a brighter ring, lower it for longer
 * battery life. The current limit is still enforced at any setting.
 */
#define BCM_CAP_Q8 77

enum { BCM_PORT_A = 0, BCM_PORT_B = 1, BCM_PORT_C = 2 };

/*
 * One bit-plane: per-port masks of LED pins to drive low (clr, written to
 * GPIOx->BRR) and high (set, written to GPIOx->BSRR). The ISR writes all
 * clears before any sets, so no intermediate state exceeds the limit.
 */
typedef struct {
    uint32_t clr[BCM_PORTS];
    uint32_t set[BCM_PORTS];
} bcm_plane_t;

typedef struct {
    bcm_plane_t plane[BCM_BITS]; /* plane[b] lasts 2^b LSB slices */
} bcm_frame_t;

/*
 * Build a frame from 16-bit linear duty values (0 = off, 65535 = full).
 * Each LED's 8-bit code is duty * cap_q8 / 65536. If any bit-plane would
 * have more than BCM_MAX_ON LEDs on, the whole frame is scaled down until
 * none does. Returns the cap actually applied (== cap_q8 when no scaling
 * was needed, smaller when the current limiter stepped in).
 */
uint8_t bcm_build(const uint16_t duty[BCM_LED_COUNT], uint8_t cap_q8,
                  bcm_frame_t *out);

/* Mask of the LED pins on a port (BCM_PORT_A..C). */
uint32_t bcm_port_led_mask(int port);

#endif
