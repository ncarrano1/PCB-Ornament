/*
 * fx.c - fixed-point helpers and drawing primitives. Pure C. See fx.h.
 */
#include "fx.h"
#include "tables.h"

int16_t height(int i)
{
    /* Fold to 0..half a turn so both sides give exactly the same value. */
    uint32_t a = angle_from_top(i);
    if (a > RING_POS / 2) {
        a = RING_POS - a;
    }
    return cos16(ring_to_angle(a));
}

int16_t sin16(uint16_t angle)
{
    uint32_t idx = angle >> 8;
    int32_t frac = angle & 0xFF;
    int32_t a = TBL_SINE[idx];
    int32_t b = TBL_SINE[(idx + 1U) & 0xFFU];
    return (int16_t)(a + (((b - a) * frac) >> 8));
}

uint16_t exp_neg(uint32_t x_q8)
{
    /* Table step is 1/32 e-fold = 8 units of x_q8. */
    uint32_t idx = x_q8 >> 3;
    if (idx >= 255U) {
        return 0;
    }
    int32_t frac = (int32_t)(x_q8 & 7U);
    int32_t a = TBL_EXP_NEG[idx];
    int32_t b = TBL_EXP_NEG[idx + 1U];
    return (uint16_t)(a + (((b - a) * frac) >> 3));
}

uint32_t isqrt32(uint32_t x)
{
    uint32_t r = 0, bit = 1UL << 30;
    while (bit > x) {
        bit >>= 2;
    }
    while (bit) {
        if (x >= r + bit) {
            x -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return r;
}

uint16_t smooth16(uint16_t t)
{
    /* 3t^2 - 2t^3 in Q16 */
    uint32_t t2 = ((uint32_t)t * t) >> 16;
    uint32_t t3 = (t2 * t) >> 16;
    int32_t r = (int32_t)(3U * t2) - (int32_t)(2U * t3);
    if (r < 0) r = 0;
    if (r > 65535) r = 65535;
    return (uint16_t)r;
}

void fx_clear(fx_buf_t buf)
{
    for (int i = 0; i < RING_LEDS; i++) {
        buf[i] = 0;
    }
}

void fx_add(fx_buf_t buf, int i, uint32_t v)
{
    uint32_t s = buf[i] + v;
    buf[i] = (uint16_t)(s > 0xFFFFU ? 0xFFFFU : s);
}

void fx_max(fx_buf_t buf, int i, uint32_t v)
{
    if (v > buf[i]) {
        buf[i] = (uint16_t)(v > 0xFFFFU ? 0xFFFFU : v);
    }
}

void fx_point(fx_buf_t buf, uint32_t pos, uint8_t v)
{
    pos %= RING_POS;
    int32_t n = (int32_t)(pos / RING_POS_PER_LED);
    uint32_t f = pos % RING_POS_PER_LED; /* 0..255 toward the next LED */
    /* Split linear light (1-f : f), expressed back in perceptual units. */
    fx_add(buf, ring_led_at(n), scale8(v, TBL_INV_GAMMA[255U - f]));
    fx_add(buf, ring_led_at(n + 1), scale8(v, TBL_INV_GAMMA[f]));
}

void fx_blob(fx_buf_t buf, uint32_t pos, uint32_t radius, uint8_t v)
{
    if (radius == 0) {
        return;
    }
    for (int i = 0; i < RING_LEDS; i++) {
        uint32_t d = ring_dist(ring_led_pos(i), pos % RING_POS);
        if (d >= radius) {
            continue;
        }
        /* (1 + cos(pi * d / r)) / 2 */
        uint16_t a = (uint16_t)((d * 32768U) / radius);
        int32_t c = cos16(a); /* Q15 */
        uint32_t shape = (uint32_t)(c + 32768) >> 1; /* 0..32767 */
        fx_add(buf, i, (v * shape + 16384U) >> 15);
    }
}

void fx_out(const fx_buf_t buf, uint8_t out[RING_LEDS])
{
    for (int i = 0; i < RING_LEDS; i++) {
        out[i] = (uint8_t)(buf[i] > 255U ? 255U : buf[i]);
    }
}

void fx_comet(fx_buf_t buf, uint32_t head, int dir, uint32_t tail_leds,
              uint32_t tail_max, uint8_t peak)
{
    const uint32_t tail_len = tail_leds * RING_POS_PER_LED;
    const uint32_t cut = tail_max * RING_POS_PER_LED;
    for (int i = 0; i < RING_LEDS; i++) {
        uint32_t p = ring_led_pos(i);
        /* Distance behind the head, measured against the direction of travel. */
        uint32_t behind = dir > 0 ? ring_cw(p, head) : ring_cw(head, p);
        uint32_t ahead = RING_POS - behind;
        uint32_t v = 0;
        if (behind < cut) {
            v = ((uint32_t)peak * exp_neg(behind * 256U / tail_len) + 32767U) / 65535U;
            /* fade the very end so the cut-off never pops */
            if (behind > cut - RING_POS_PER_LED * 2U) {
                v = v * (cut - behind) / (RING_POS_PER_LED * 2U);
            }
        }
        if (ahead < RING_POS_PER_LED) {
            /* leading edge: the LED the head is moving into */
            uint32_t lead = scale8(peak, 255U - (uint8_t)ahead);
            if (lead > v) v = lead;
        }
        if (v > buf[i]) buf[i] = (uint16_t)v;
    }
}
