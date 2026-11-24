/*
 * mode_table.c - the button cycles through these in order.
 * Adding a mode: one new file in src/modes/ plus one line here.
 */
#include "mode.h"

extern const mode_def_t mode_wave;
extern const mode_def_t mode_comet;
extern const mode_def_t mode_binary;
extern const mode_def_t mode_twinkle;
extern const mode_def_t mode_pendulum;
extern const mode_def_t mode_rain;
extern const mode_def_t mode_heart;
extern const mode_def_t mode_candle;
extern const mode_def_t mode_plasma;
extern const mode_def_t mode_fireworks;
extern const mode_def_t mode_standing;
extern const mode_def_t mode_marble;
extern const mode_def_t mode_snow;
extern const mode_def_t mode_hourglass;
extern const mode_def_t mode_lightning;
extern const mode_def_t mode_fireflies;
extern const mode_def_t mode_ripples;
extern const mode_def_t mode_automaton;
extern const mode_def_t mode_marquee;
extern const mode_def_t mode_ember;

const mode_def_t *const MODES[] = {
    &mode_wave,    /* 1 */
    &mode_comet,   /* 2 */
    &mode_binary,  /* 3 */
    &mode_twinkle, /* 4 */
    &mode_pendulum,/* 5 */
    &mode_rain,    /* 6 */
    &mode_heart,   /* 7 */
    &mode_candle,  /* 8 */
    &mode_plasma,  /* 9 */
    &mode_fireworks, /* 10 */
    &mode_standing,  /* 11 */
    &mode_marble,    /* 12 */
    &mode_snow,      /* 13 */
    &mode_hourglass, /* 14 */
    &mode_lightning, /* 15 */
    &mode_fireflies, /* 16 */
    &mode_ripples,   /* 17 */
    &mode_automaton, /* 18 */
    &mode_marquee,   /* 19 */
    &mode_ember,     /* 20 */
};

const uint8_t MODE_COUNT = (uint8_t)(sizeof(MODES) / sizeof(MODES[0]));
