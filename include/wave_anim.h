/*
 * wave_anim.h - traveling-wave frame generator. Pure C, no HAL.
 * Produces 16-bit linear duty per LED; the brightness layer (bcm_frame)
 * applies the global cap and the current limit.
 */
#ifndef WAVE_ANIM_H
#define WAVE_ANIM_H

#include <stdint.h>
#include "bcm_frame.h"

/* Positions are in 1/256 of an LED; one full turn of the ring: */
#define WAVE_POS_PER_LED 256
#define WAVE_RING_POS    (BCM_LED_COUNT * WAVE_POS_PER_LED)

typedef enum {
    WAVE_SLOW,  /* one bump, slow */
    WAVE_FAST,  /* one bump, fast */
    WAVE_TWO,   /* two bumps on opposite sides of the ring */
    WAVE_OFF,   /* all LEDs off */
    WAVE_MODE_COUNT
} wave_mode_t;

/* Build the bump lookup table. Call once at startup. */
void wave_anim_init(void);

/* Move the wave forward by dt_ms at the speed of `mode`. */
void wave_anim_advance(wave_mode_t mode, uint32_t dt_ms);

/* Render the current position into duty[]. */
void wave_anim_render(wave_mode_t mode, uint16_t duty[BCM_LED_COUNT]);

/* Render at an explicit position 0..WAVE_RING_POS-1 (used by tests). */
void wave_anim_render_at(wave_mode_t mode, uint32_t pos,
                         uint16_t duty[BCM_LED_COUNT]);

#endif
