/*
 * wave.c - a smooth traveling wave of brightness around the ring.
 *
 * Press SW1 to cycle: slow wave -> fast wave -> two opposite waves -> off.
 * Brightness, the 30 % cap and the per-slice current limit are handled by
 * the BCM engine (led_bcm / bcm_frame); shape and speed live in wave_anim.c.
 */
#include "board.h"
#include "led_bcm.h"
#include "wave_anim.h"

#define FRAME_MS    20  /* 50 Hz animation updates */
#define DEBOUNCE_MS 30

int main(void)
{
    board_init();
    wave_anim_init();
    bcm_start();

    wave_mode_t mode = WAVE_SLOW;
    uint16_t duty[LED_COUNT];
    bool was_pressed = false;
    uint32_t last_frame = HAL_GetTick();
    uint32_t last_button_change = 0;

    while (1) {
        uint32_t now = HAL_GetTick();

        /* Advance and redraw the wave. */
        if (now - last_frame >= FRAME_MS) {
            uint32_t dt = now - last_frame;
            last_frame = now;
            if (mode != WAVE_OFF) {
                wave_anim_advance(mode, dt);
                wave_anim_render(mode, duty);
                bcm_submit(duty);
            }
        }

        /* Change mode on each new button press (with simple debounce). */
        bool pressed = button_is_pressed();
        if (pressed != was_pressed && now - last_button_change >= DEBOUNCE_MS) {
            last_button_change = now;
            was_pressed = pressed;
            if (pressed) {
                mode = (wave_mode_t)((mode + 1) % WAVE_MODE_COUNT);
                if (mode == WAVE_OFF) {
                    bcm_stop();
                } else {
                    bcm_start();
                }
            }
        }

        bcm_idle(); /* sleep until the next timer or SysTick interrupt */
    }
}
