/*
 * button_fsm.h - debounced short / double / long press detection. Pure C.
 */
#ifndef BUTTON_FSM_H
#define BUTTON_FSM_H

#include <stdbool.h>
#include <stdint.h>

#define BTN_DEBOUNCE_MS 25
#define BTN_LONG_MS     1000
#define BTN_DOUBLE_MS   350   /* max gap between the two presses of a double */

typedef enum { BTN_NONE, BTN_SHORT, BTN_DOUBLE, BTN_LONG } btn_event_t;

typedef struct {
    bool raw;           /* last raw sample */
    bool stable;        /* debounced state */
    bool long_fired;    /* long press already reported for this hold */
    bool ignore;        /* ignore this press until released (e.g. the wake press) */
    bool pending;       /* a short press is waiting to see if a second follows */
    bool second;        /* the current press is the second of a double */
    uint32_t raw_ms;    /* when raw last changed */
    uint32_t press_ms;  /* when the debounced press began */
    uint32_t release_ms;
} btn_t;

/* If the button is already down (e.g. it woke us), that press is ignored. */
void btn_init(btn_t *b, bool pressed, uint32_t now_ms);

/*
 * Feed a raw sample (true = pressed), once per millisecond.
 *   SHORT  - one short press (reported BTN_DOUBLE_MS after release, once it
 *            is clear no second press is coming)
 *   DOUBLE - two short presses in quick succession
 *   LONG   - as soon as a hold reaches BTN_LONG_MS
 */
btn_event_t btn_update(btn_t *b, bool pressed, uint32_t now_ms);

#endif
