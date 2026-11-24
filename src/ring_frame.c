/*
 * ring_frame.c - banked BCM brightness layer. Pure C. See ring_frame.h.
 */
#include "ring_frame.h"
#include "led_pins.h"
#include "tables.h"

typedef struct {
    uint8_t port;
    uint8_t bit;
} led_gpio_t;

#define LED_PIN(port, bit) {RING_PORT_##port, bit},
static const led_gpio_t MAP[RING_LEDS] = {
    LED_PIN_MAP
};
#undef LED_PIN

uint32_t ring_port_bank_mask(int port, int bank)
{
    uint32_t m = 0;
    for (int i = 0; i < RING_LEDS; i++) {
        if (MAP[i].port == port && ring_led_bank(i) == bank) {
            m |= 1UL << MAP[i].bit;
        }
    }
    return m;
}

uint32_t ring_port_mask(int port)
{
    return ring_port_bank_mask(port, 0) | ring_port_bank_mask(port, 1);
}

void ring_codes(const uint8_t bright[RING_LEDS], uint8_t code[RING_LEDS])
{
    for (int i = 0; i < RING_LEDS; i++) {
        uint32_t lin = TBL_GAMMA[bright[i]];
        code[i] = (uint8_t)((lin * RING_CAP_CODE + 32767U) / 65535U);
    }
}

void ring_build(const uint8_t code[RING_LEDS], ring_frame_t *out)
{
    uint32_t all[RING_PORTS];
    for (int p = 0; p < RING_PORTS; p++) {
        all[p] = ring_port_mask(p);
    }

    for (int s = 0; s < RING_SLICES; s++) {
        const int bank = ring_slice_bank(s);
        const int bit = ring_slice_bit(s);
        uint32_t set[RING_PORTS] = {0, 0, 0};

        /* Only this slice's bank can be set: at most 20 LEDs, by construction. */
        for (int i = bank; i < RING_LEDS; i += 2) {
            if ((code[i] >> bit) & 1U) {
                set[MAP[i].port] |= 1UL << MAP[i].bit;
            }
        }
        for (int p = 0; p < RING_PORTS; p++) {
            out->slice[s].set[p] = set[p];
            out->slice[s].clr[p] = all[p] & ~set[p];
        }
    }
}
