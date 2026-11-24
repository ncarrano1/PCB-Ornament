/*
 * ring_engine.h - banked BCM refresh on TIM2 (board layer).
 * 14 slices per frame (7 bit-planes x 2 banks), 246 Hz at the 8 MHz HSI.
 */
#ifndef RING_ENGINE_H
#define RING_ENGINE_H

#include <stdint.h>
#include "ring_frame.h"

/* Turn every LED off and start the refresh timer. Call after board_init(). */
void ring_engine_start(void);

/* Stop the refresh timer and turn every LED off. */
void ring_engine_stop(void);

/*
 * Queue a frame of perceptual brightness (0..255 per LED). Gamma and the
 * brightness cap are applied here. Sleeps until the previous frame has been
 * picked up, so the scan never shows a half-updated frame.
 */
void ring_engine_submit(const uint8_t bright[RING_LEDS]);

/* Sleep the CPU until the next interrupt. */
void ring_engine_idle(void);

/* Worst-case cycles spent building a frame (gamma + planes), for profiling. */
extern volatile uint32_t g_build_cycles_max;

#endif
