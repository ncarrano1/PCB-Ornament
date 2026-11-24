/*
 * ring_geom.h - ring geometry for effects. Pure C.
 *
 * LED index i = 0..39 is D(i+1), clockwise viewed from the LED side,
 * 9 degrees apart. Index 39 (D40) is 12 o'clock (top, where the ornament
 * hangs); index 19 (D20) is 6 o'clock (bottom). Indices 0..18 run down the
 * right side, 20..38 up the left side.
 *
 * Positions are in "ring units": 256 per LED, RING_POS per turn, measured
 * clockwise from the top. LED i sits at ring_led_pos(i).
 */
#ifndef RING_GEOM_H
#define RING_GEOM_H

#include <stdint.h>
#include "ring_frame.h"

#define RING_POS_PER_LED 256
#define RING_POS         (RING_LEDS * RING_POS_PER_LED) /* 10240 = one turn */
#define RING_POS_TOP     0
#define RING_POS_BOTTOM  (RING_POS / 2)
#define RING_LED_TOP     39
#define RING_LED_BOTTOM  19

/* Angle of LED i clockwise from the top, in ring units (0 = top). */
static inline uint32_t angle_from_top(int i)
{
    return (uint32_t)((i + 1) % RING_LEDS) * RING_POS_PER_LED;
}

/* Position of LED i (same as its angle from the top). */
static inline uint32_t ring_led_pos(int i) { return angle_from_top(i); }

/* LED index at whole-LED step n (n = 0 is the top LED, D40). */
static inline int ring_led_at(int32_t n)
{
    int32_t m = (n - 1) % RING_LEDS;
    return (int)(m < 0 ? m + RING_LEDS : m);
}

/* Height of LED i: cos(angle from top), Q15. Top = +32767, bottom = -32767. */
int16_t height(int i);

/* The LED at the same height on the other side of the ring. */
static inline int mirror(int i)
{
    int m = (38 - i) % RING_LEDS;
    return m < 0 ? m + RING_LEDS : m;
}

/* Wrap any signed position into 0..RING_POS-1. */
static inline uint32_t ring_wrap(int32_t p)
{
    int32_t m = p % RING_POS;
    return (uint32_t)(m < 0 ? m + RING_POS : m);
}

/* Clockwise distance from a to b, 0..RING_POS-1. */
static inline uint32_t ring_cw(uint32_t a, uint32_t b)
{
    return (b + RING_POS - a) % RING_POS;
}

/* Shortest distance between two positions, 0..RING_POS/2. */
static inline uint32_t ring_dist(uint32_t a, uint32_t b)
{
    uint32_t d = ring_cw(a, b);
    return d > RING_POS / 2 ? RING_POS - d : d;
}

#endif
