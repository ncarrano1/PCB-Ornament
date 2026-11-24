/*
 * mode.h - LED mode interface. Pure C (modes also build on the host).
 *
 * To add a mode: create src/modes/mode_<name>.c defining
 *     const mode_def_t mode_<name> = {"name", init, update};
 * and add &mode_<name> to the table in src/modes/mode_table.c.
 */
#ifndef MODE_H
#define MODE_H

#include <stdint.h>
#include "ring_frame.h"

typedef struct {
    const char *name;
    /* Reset all state. `seed` seeds the mode's own PRNG. */
    void (*init)(uint32_t seed);
    /* Advance by frame_ms and write perceptual brightness 0..255 per LED. */
    void (*update)(uint32_t frame_ms, uint8_t out[RING_LEDS]);
} mode_def_t;

extern const mode_def_t *const MODES[];
extern const uint8_t MODE_COUNT;

#endif
