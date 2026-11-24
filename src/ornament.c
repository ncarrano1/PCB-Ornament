/*
 * ornament.c - final ornament firmware: 10 LED modes on the button.
 *
 * Short press: next mode. (Stage 4 adds long press = sleep, auto-sleep
 * and mode memory.)
 */
#include "board.h"
#include "ring_engine.h"
#include "ornament_core.h"
#include "button_fsm.h"

/* Worst-case cycles for one animation step (all modes + crossfade). */
volatile uint32_t g_frame_cycles_max;

int main(void)
{
    board_init();
    ring_engine_start(); /* also enables the DWT cycle counter */

    core_init(0x0E7A2024U, 0);

    btn_t btn;
    uint32_t now = HAL_GetTick();
    btn_init(&btn, button_is_pressed(), now);
    uint32_t next_frame = now;
    uint32_t last_ms = now;
    uint8_t out[RING_LEDS];

    while (1) {
        now = HAL_GetTick();

        /* Fixed-rate animation, independent of the BCM refresh. */
        if ((int32_t)(now - next_frame) >= 0) {
            next_frame += CORE_FRAME_MS;
            if ((int32_t)(now - next_frame) > 100) {
                next_frame = now; /* fell far behind: don't spin catching up */
            }
            uint32_t t0 = DWT->CYCCNT;
            core_frame(out);
            uint32_t dt = DWT->CYCCNT - t0;
            if (dt > g_frame_cycles_max) {
                g_frame_cycles_max = dt;
            }
            ring_engine_submit(out);
        }

        /* Button, sampled once per millisecond tick. */
        if (now != last_ms) {
            last_ms = now;
            btn_event_t ev = btn_update(&btn, button_is_pressed(), now);
            if (ev == BTN_SHORT) {
                core_stir(now);
                core_next_mode();
            }
        }

        ring_engine_idle(); /* wake on the next TIM2 or SysTick interrupt */
    }
}
