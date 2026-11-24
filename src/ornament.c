/*
 * ornament.c - final ornament firmware: 20 LED modes on one button.
 *
 *   short press  next mode (wraps to 1)
 *   double press back to mode 1
 *   long press   sleep (any press wakes it)
 *
 * At power-up and on wake, a battery gauge fills the ring from the bottom,
 * then the mode fades in. After 20 minutes without a press it sleeps on its
 * own. The selected mode survives power-off (saved to flash on sleep, and
 * 60 s after a change).
 */
#include "board.h"
#include "ring_engine.h"
#include "ornament_core.h"
#include "button_fsm.h"
#include "gauge.h"
#include "mode.h"
#include "power.h"

/* ---- Tweakable ------------------------------------------------------- */
#define AUTO_SLEEP_MS   (20UL * 60UL * 1000UL)  /* sleep after no press for this long */
#define SAVE_AFTER_MS   60000UL  /* save a newly chosen mode once it has been kept this long */
#define SLEEP_FADE_MS   250      /* fade to dark before sleeping */
#define WAKE_CONFIRM_MS 30       /* button must still be down this long to count as a wake */
/*
 * Never enter Stop within this long of a reset. SWD can't reach a stopped
 * chip and NRST isn't wired to the programmer, so this guarantees a window
 * to connect after every power-up, even if sleep code misbehaves.
 */
#define STOP_GUARD_MS   10000UL
/* ---------------------------------------------------------------------- */

static int16_t stored_mode = -1; /* what flash holds, -1 = nothing yet */

/* Worst-case cycles for one animation step (all modes + crossfade). */
volatile uint32_t g_frame_cycles_max;

static void save_mode(uint8_t mode, bool leds_on)
{
    if (stored_mode == mode) {
        return;
    }
    /* A page erase stalls the CPU for ~20-40 ms; blank the LEDs for it
     * rather than freeze a slice on. Happens once per 512 saves. */
    bool erase = leds_on && power_save_needs_erase();
    if (erase) ring_engine_stop();
    power_save_mode(mode);
    if (erase) ring_engine_start();
    stored_mode = mode;
}

static void wait_ms(uint32_t ms)
{
    uint32_t t0 = HAL_GetTick();
    while (HAL_GetTick() - t0 < ms) {
        ring_engine_idle();
    }
}

/* Start (or restart after wake): gauge, then the current mode. */
static void show_start(uint8_t mode)
{
    uint32_t vdd = power_vdd_mv(); /* measured with the LEDs off: unloaded */
    ring_engine_start();
    core_init(0x0E7A2024U ^ vdd ^ HAL_GetTick(), mode);
    core_show_gauge(gauge_percent(vdd));
}

static void go_to_sleep(void)
{
    /* Fade out the current picture. */
    uint8_t out[RING_LEDS];
    for (uint32_t t = 0; t < SLEEP_FADE_MS; t += CORE_FRAME_MS) {
        core_frame(out);
        uint32_t k = 256U - t * 256U / SLEEP_FADE_MS;
        for (int i = 0; i < RING_LEDS; i++) out[i] = (uint8_t)((out[i] * k) >> 8);
        ring_engine_submit(out);
        wait_ms(CORE_FRAME_MS);
    }
    ring_engine_stop();
    save_mode(core_mode(), false);

    for (;;) {
        /* Wait for the button to be released and settled, so contact
         * bounce on release can't wake us straight away... */
        uint32_t up_since = HAL_GetTick();
        while (HAL_GetTick() - up_since < 60U) {
            if (button_is_pressed()) up_since = HAL_GetTick();
            ring_engine_idle();
        }
        /* ...and never stop inside the post-reset SWD window. */
        while (HAL_GetTick() < STOP_GUARD_MS) {
            ring_engine_idle();
        }

        power_stop_until_button();

        /* Only a real press wakes us (not a glitch on the line). */
        wait_ms(WAKE_CONFIRM_MS);
        if (button_is_pressed()) {
            return;
        }
    }
}

int main(void)
{
    board_init();
    power_init();

    stored_mode = (int16_t)power_load_mode(MODE_COUNT);
    show_start(stored_mode >= 0 ? (uint8_t)stored_mode : 0);

    btn_t btn;
    uint32_t now = HAL_GetTick();
    btn_init(&btn, button_is_pressed(), now);
    uint32_t next_frame = now;
    uint32_t last_ms = now;
    uint32_t last_press = now;
    uint32_t changed_at = now;
    uint8_t out[RING_LEDS];

    while (1) {
        now = HAL_GetTick();

        /* Fixed-rate animation, independent of the BCM refresh. */
        if ((int32_t)(now - next_frame) >= 0) {
            next_frame += CORE_FRAME_MS;
            if ((int32_t)(now - next_frame) > 100) {
                next_frame = now; /* fell far behind: don't spin catching up */
            }
            uint32_t t0 = ring_engine_cycles();
            core_frame(out);
            uint32_t dt = ring_engine_cycles() - t0;
            if (dt > g_frame_cycles_max) g_frame_cycles_max = dt;
            ring_engine_submit(out);
        }

        /* Button, sampled once per millisecond tick. */
        if (now != last_ms) {
            last_ms = now;
            btn_event_t ev = btn_update(&btn, button_is_pressed(), now);
            if (ev != BTN_NONE) {
                last_press = now;
                core_stir(now);
            }
            if (ev == BTN_SHORT) {
                core_next_mode();
                changed_at = now;
            } else if (ev == BTN_DOUBLE) {
                core_set_mode(0);
                changed_at = now;
            } else if (ev == BTN_LONG) {
                go_to_sleep();
                /* Woken by a press that is still held: ignore it. */
                show_start(core_mode());
                now = HAL_GetTick();
                btn_init(&btn, true, now);
                next_frame = last_ms = last_press = changed_at = now;
                continue;
            }
        }

        /* Remember a newly chosen mode once it has been kept a while. */
        if (stored_mode != core_mode() && now - changed_at >= SAVE_AFTER_MS) {
            save_mode(core_mode(), true);
        }

        /* Auto-sleep. */
        if (now - last_press >= AUTO_SLEEP_MS) {
            go_to_sleep();
            show_start(core_mode());
            now = HAL_GetTick();
            btn_init(&btn, true, now);
            next_frame = last_ms = last_press = changed_at = now;
            continue;
        }

        ring_engine_idle(); /* wake on the next TIM2 or SysTick interrupt */
    }
}
