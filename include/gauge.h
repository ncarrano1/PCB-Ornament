/*
 * gauge.h - battery gauge shown at power-up and wake. Pure C.
 * The ring fills up from the bottom like a liquid level, holds, then the
 * selected mode fades in over it.
 */
#ifndef GAUGE_H
#define GAUGE_H

#include <stdint.h>
#include "ring_frame.h"

/* Rough CR2477 charge from its unloaded voltage (mV), 0..100 %. */
uint8_t gauge_percent(uint32_t vdd_mv);

/* Start showing `pct`. */
void gauge_start(uint8_t pct);

/*
 * Advance and draw the gauge. Returns how much of the next mode should be
 * blended over it (0..256); 256 means the gauge is finished.
 */
uint32_t gauge_update(uint32_t frame_ms, uint8_t out[RING_LEDS]);

#endif
