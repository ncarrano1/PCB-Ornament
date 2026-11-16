/*
 * led_pins.h - LED pin map, the single source of truth for D1..D40.
 * Pin map taken from the Altium PCB (R1..R40 feed D1..D40).
 *
 * X-macro: define LED_PIN(port, bit) before expanding LED_PIN_MAP.
 * `port` is the bare letter A/B/C, `bit` the pin number.
 * No HAL includes here, so host-side tests can use the same map.
 */
#ifndef LED_PINS_H
#define LED_PINS_H

/* Index 0 = D1 ... index 39 = D40, in ring order. */
#define LED_PIN_MAP \
    LED_PIN(A, 0)  /* D1  */ \
    LED_PIN(C, 2)  /* D2  */ \
    LED_PIN(A, 1)  /* D3  */ \
    LED_PIN(C, 3)  /* D4  */ \
    LED_PIN(A, 2)  /* D5  */ \
    LED_PIN(C, 4)  /* D6  */ \
    LED_PIN(A, 3)  /* D7  */ \
    LED_PIN(C, 5)  /* D8  */ \
    LED_PIN(A, 4)  /* D9  */ \
    LED_PIN(C, 6)  /* D10 */ \
    LED_PIN(A, 5)  /* D11 */ \
    LED_PIN(C, 7)  /* D12 */ \
    LED_PIN(A, 6)  /* D13 */ \
    LED_PIN(C, 8)  /* D14 */ \
    LED_PIN(A, 7)  /* D15 */ \
    LED_PIN(C, 9)  /* D16 */ \
    LED_PIN(A, 8)  /* D17 */ \
    LED_PIN(C, 10) /* D18 */ \
    LED_PIN(A, 9)  /* D19 */ \
    LED_PIN(C, 11) /* D20 */ \
    LED_PIN(A, 10) /* D21 */ \
    LED_PIN(C, 12) /* D22 */ \
    LED_PIN(A, 11) /* D23 */ \
    LED_PIN(B, 0)  /* D24 */ \
    LED_PIN(A, 12) /* D25 */ \
    LED_PIN(B, 1)  /* D26 */ \
    LED_PIN(A, 15) /* D27 - JTDI after reset, freed in board_init */ \
    LED_PIN(B, 2)  /* D28 - also BOOT1, fine as output */ \
    LED_PIN(C, 0)  /* D29 */ \
    LED_PIN(B, 3)  /* D30 - JTDO after reset, freed in board_init */ \
    LED_PIN(C, 1)  /* D31 */ \
    LED_PIN(B, 4)  /* D32 - NJTRST after reset, freed in board_init */ \
    LED_PIN(B, 8)  /* D33 */ \
    LED_PIN(B, 5)  /* D34 */ \
    LED_PIN(B, 9)  /* D35 */ \
    LED_PIN(B, 6)  /* D36 */ \
    LED_PIN(B, 10) /* D37 */ \
    LED_PIN(B, 7)  /* D38 */ \
    LED_PIN(B, 11) /* D39 */ \
    LED_PIN(B, 12) /* D40 */

#endif
