/*
 * led_bcm.h - timer-driven BCM brightness engine (board layer).
 *
 * TIM2 steps through 8 bit-planes per frame at about 245 Hz. Frames are
 * double-buffered and swapped only between frames. The per-plane
 * current limit is enforced by bcm_build() (see bcm_frame.h).
 */
#ifndef LED_BCM_H
#define LED_BCM_H

#include <stdint.h>
#include "bcm_frame.h"

/* Turn all LEDs off and start the refresh timer. Call after board_init(). */
void bcm_start(void);

/* Stop the refresh timer and turn every LED off. */
void bcm_stop(void);

/*
 * Queue a new frame of 16-bit duty values. Waits (sleeping) until the
 * previous frame has been picked up, so call it at the animation rate.
 * Returns the brightness cap applied (less than BCM_CAP_Q8 if the
 * current limiter had to dim this frame).
 */
uint8_t bcm_submit(const uint16_t duty[BCM_LED_COUNT]);

/* Sleep the CPU until the next interrupt (timer, SysTick). */
void bcm_idle(void);

#endif
