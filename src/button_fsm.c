/*
 * button_fsm.c - debounced short / double / long press detection. Pure C.
 */
#include "button_fsm.h"

void btn_init(btn_t *b, bool pressed, uint32_t now_ms)
{
    b->raw = pressed;
    b->stable = pressed;
    b->long_fired = false;
    b->ignore = pressed;
    b->pending = false;
    b->second = false;
    b->raw_ms = now_ms;
    b->press_ms = now_ms;
    b->release_ms = now_ms;
}

btn_event_t btn_update(btn_t *b, bool pressed, uint32_t now_ms)
{
    if (pressed != b->raw) {
        b->raw = pressed;
        b->raw_ms = now_ms;
    }

    if (b->raw != b->stable && now_ms - b->raw_ms >= BTN_DEBOUNCE_MS) {
        b->stable = b->raw;
        if (b->stable) {
            /* press */
            b->press_ms = now_ms;
            b->long_fired = false;
            if (b->pending) {
                b->pending = false;
                b->second = true;
            }
        } else {
            /* release */
            bool was_second = b->second;
            b->second = false;
            if (b->ignore) {
                b->ignore = false;
            } else if (!b->long_fired) {
                if (was_second) {
                    return BTN_DOUBLE;
                }
                b->pending = true;
                b->release_ms = now_ms;
            }
        }
    }

    if (b->pending && !b->stable && now_ms - b->release_ms >= BTN_DOUBLE_MS) {
        b->pending = false;
        return BTN_SHORT;
    }

    if (b->stable && !b->ignore && !b->long_fired && now_ms - b->press_ms >= BTN_LONG_MS) {
        b->long_fired = true;
        b->second = false;
        return BTN_LONG;
    }
    return BTN_NONE;
}
