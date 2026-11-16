/*
 * bcm_frame.c - brightness layer for BCM. Pure C, no HAL.
 * See bcm_frame.h. Enforces the per-bit-plane current limit (rule 1).
 */
#include "bcm_frame.h"
#include "led_pins.h"

typedef struct {
    uint8_t port; /* BCM_PORT_A..C */
    uint8_t bit;  /* pin number 0..15 */
} led_gpio_t;

#define LED_PIN(port, bit) {BCM_PORT_##port, bit},
static const led_gpio_t MAP[BCM_LED_COUNT] = {
    LED_PIN_MAP
};
#undef LED_PIN

uint32_t bcm_port_led_mask(int port)
{
    uint32_t mask = 0;
    for (int i = 0; i < BCM_LED_COUNT; i++) {
        if (MAP[i].port == port) {
            mask |= 1UL << MAP[i].bit;
        }
    }
    return mask;
}

/* Convert duty to 8-bit codes with an effective cap. */
static void make_codes(const uint16_t duty[BCM_LED_COUNT], uint32_t cap,
                       uint8_t code[BCM_LED_COUNT])
{
    for (int i = 0; i < BCM_LED_COUNT; i++) {
        uint32_t c = ((uint32_t)duty[i] * cap + 32768UL) >> 16;
        code[i] = (uint8_t)(c > 255 ? 255 : c);
    }
}

/* True if every bit-plane has at most BCM_MAX_ON LEDs on. */
static int planes_within_limit(const uint8_t code[BCM_LED_COUNT])
{
    for (int b = 0; b < BCM_BITS; b++) {
        int on = 0;
        for (int i = 0; i < BCM_LED_COUNT; i++) {
            on += (code[i] >> b) & 1;
        }
        if (on > BCM_MAX_ON) {
            return 0;
        }
    }
    return 1;
}

uint8_t bcm_build(const uint16_t duty[BCM_LED_COUNT], uint8_t cap_q8,
                  bcm_frame_t *out)
{
    uint8_t code[BCM_LED_COUNT];
    uint32_t cap = cap_q8;

    /*
     * Current limit. Scaling isn't monotonic per bit (e.g. 64 -> 63 sets
     * six bits), so recount the real bit patterns after every step.
     * Terminates: once cap reaches 0 every code is 0.
     */
    make_codes(duty, cap, code);
    while (!planes_within_limit(code)) {
        cap = (cap * 15) / 16;
        make_codes(duty, cap, code);
    }

    for (int b = 0; b < BCM_BITS; b++) {
        uint32_t set[BCM_PORTS] = {0, 0, 0};
        for (int i = 0; i < BCM_LED_COUNT; i++) {
            if ((code[i] >> b) & 1) {
                set[MAP[i].port] |= 1UL << MAP[i].bit;
            }
        }
        for (int p = 0; p < BCM_PORTS; p++) {
            out->plane[b].set[p] = set[p];
            out->plane[b].clr[p] = bcm_port_led_mask(p) & ~set[p];
        }
    }
    return (uint8_t)cap;
}
