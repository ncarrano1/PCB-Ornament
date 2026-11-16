/*
 * board.h - PCB Ornament hardware layer (STM32F103RBT6, 40 green LEDs)
 *
 * Every LED goes GPIO -> 220 ohm -> LED anode, cathode to GND.
 * GPIO high = LED on, about 5 mA per LED from a 3.0 V coin cell.
 */
#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32f1xx_hal.h"

#define LED_COUNT 40

/*
 * CURRENT LIMIT - do not raise without re-checking the math.
 * STM32F103 absolute max total current into VDD is 150 mA.
 * 40 LEDs x 5 mA = 200 mA would exceed it, and the resistors can't change.
 * 20 LEDs x 5 mA = 100 mA leaves margin for the MCU itself (~5 mA).
 * led_set() refuses to turn on more than this many LEDs at once.
 */
#define LED_MAX_ON 20

/* Call once at the start of main(). Sets up HAL, GPIO and the button. */
void board_init(void);

/*
 * Turn LED `index` on or off. index 0..39 is D1..D40, in ring order
 * (clockwise viewed from the LED side, D40 at 12 o'clock).
 * Returns false (and leaves the LED off) if turning it on would
 * exceed LED_MAX_ON.
 */
bool led_set(uint8_t index, bool on);

void led_all_off(void);
uint8_t led_count_on(void);

/* Mode button SW1 on PB15, active low. True while held down. */
bool button_is_pressed(void);

#endif
