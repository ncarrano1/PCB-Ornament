/*
 * mode_table.c - the button cycles through these in order.
 * Adding a mode: one new file in src/modes/ plus one line here.
 */
#include "mode.h"

extern const mode_def_t mode_wave;
extern const mode_def_t mode_comet;
extern const mode_def_t mode_binary;

const mode_def_t *const MODES[] = {
    &mode_wave,    /* 1 */
    &mode_comet,   /* 2 */
    &mode_binary,  /* 3 */
};

const uint8_t MODE_COUNT = (uint8_t)(sizeof(MODES) / sizeof(MODES[0]));
