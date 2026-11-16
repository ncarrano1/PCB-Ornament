/*
 * ring_chase.c - second test: one LED at a time around the ring.
 * Lights D1, D2 ... D40 in order, so a dead LED or a wrong pin
 * shows up as a gap in the chase.
 * Press SW1 to cycle through three speeds (this also tests the button).
 * Only one LED is ever on, so current stays around 5 mA.
 */
#include "board.h"

static const uint32_t STEP_MS[] = {100, 50, 25};
#define SPEED_COUNT (sizeof(STEP_MS) / sizeof(STEP_MS[0]))
#define DEBOUNCE_MS 30

int main(void)
{
    board_init();

    uint8_t led = 0;
    uint8_t speed = 0;
    bool was_pressed = false;
    uint32_t last_step = HAL_GetTick();
    uint32_t last_button_change = 0;

    led_set(led, true);

    while (1) {
        uint32_t now = HAL_GetTick();

        /* Advance the chase. */
        if (now - last_step >= STEP_MS[speed]) {
            last_step = now;
            led_set(led, false);
            led = (uint8_t)((led + 1) % LED_COUNT);
            led_set(led, true);
        }

        /* Change speed on each new button press (with simple debounce). */
        bool pressed = button_is_pressed();
        if (pressed != was_pressed && now - last_button_change >= DEBOUNCE_MS) {
            last_button_change = now;
            was_pressed = pressed;
            if (pressed) {
                speed = (uint8_t)((speed + 1) % SPEED_COUNT);
            }
        }
    }
}
