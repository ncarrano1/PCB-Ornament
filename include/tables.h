/*
 * tables.h - const lookup tables (generated into src/tables.c by
 * tools/gen_tables.py). Pure C, no HAL.
 */
#ifndef TABLES_H
#define TABLES_H

#include <stdint.h>

extern const uint16_t TBL_GAMMA[256];     /* perceptual -> linear 0..65535 */
extern const uint8_t  TBL_INV_GAMMA[256]; /* linear weight -> perceptual */
extern const int16_t  TBL_SINE[256];      /* one sine period, Q15 */
extern const uint16_t TBL_EXP_NEG[256];   /* exp(-i/32) * 65535 */

#endif
