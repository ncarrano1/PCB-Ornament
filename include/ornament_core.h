/*
 * ornament_core.h - mode switching and crossfade. Pure C, shared by the
 * firmware and the host test/preview tools.
 */
#ifndef ORNAMENT_CORE_H
#define ORNAMENT_CORE_H

#include <stdint.h>
#include "ring_frame.h"

#define CORE_FRAME_MS 16   /* fixed animation step: 62.5 fps */
#define CORE_FADE_MS  300  /* crossfade when switching modes */

/* Start in `mode` (0-based). `seed` makes every random mode deterministic. */
void core_init(uint32_t seed, uint8_t mode);

/* Switch to a mode (crossfades from the current one). */
void core_set_mode(uint8_t mode);
void core_next_mode(void);
uint8_t core_mode(void);

/* Stir extra entropy into future seeds (e.g. button-press timing). */
void core_stir(uint32_t entropy);

/* Show the battery gauge (0..100 %), then fade into the current mode. */
void core_show_gauge(uint8_t pct);

/* Advance one CORE_FRAME_MS step and write perceptual brightness. */
void core_frame(uint8_t out[RING_LEDS]);

#endif
