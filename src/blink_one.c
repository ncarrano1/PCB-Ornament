/*
 * blink_one.c - first bring-up test.
 * Blinks D1 (PA0) once a second. If this works, power, clock, SWD
 * flashing and one LED path are all good.
 */
#include "board.h"

int main(void)
{
    board_init();

    while (1) {
        led_set(0, true);
        HAL_Delay(500);
        led_set(0, false);
        HAL_Delay(500);
    }
}
