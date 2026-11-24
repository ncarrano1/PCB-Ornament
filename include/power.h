/*
 * power.h - battery measurement, sleep, and mode memory (board layer).
 */
#ifndef POWER_H
#define POWER_H

#include <stdbool.h>
#include <stdint.h>

/* Put the unused pins (PC13-15, PB13, PB14, PD2) in analog mode. */
void power_init(void);

/* Supply voltage in mV, from the internal reference (VREFINT). */
uint32_t power_vdd_mv(void);

/* Saved mode from flash, or -1 if none/invalid. */
int power_load_mode(uint8_t mode_count);

/* True if the next save has to erase the flash page first (~20-40 ms). */
bool power_save_needs_erase(void);

/* Append the mode to the flash log (erases the page when it is full). */
void power_save_mode(uint8_t mode);

/*
 * Enter Stop mode (microamps) until PB15 sees a falling edge (EXTI15).
 * LEDs must already be off and the BCM timer stopped. Returns after wake,
 * running on the 8 MHz HSI again with the HAL tick resumed.
 */
void power_stop_until_button(void);

#endif
