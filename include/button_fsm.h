/*
 * button_fsm.h - debounced short/long press detection. Pure C.
 */
#ifndef BUTTON_FSM_H
#define BUTTON_FSM_H

#include <stdbool.h>
#include <stdint.h>

#define BTN_DEBOUNCE_MS 25
#define BTN_LONG_MS     1000

typedef enum { BTN_NONE, BTN_SHORT, BTN_LONG } btn_event_t;

typedef struct {
    bool raw;           /* last raw sample */
    bool stable;        /* debounced state */
    bool long_fired;    /* long press already reported for this hold */
    bool ignore;        /* ignore this press until released (e.g. the wake press) */
    uint32_t raw_ms;    /* when raw last changed */
    uint32_t press_ms;  /* when the debounced press began */
} btn_t;

/* If the button is already down (e.g. it woke us), that press is ignored. */
void btn_init(btn_t *b, bool pressed, uint32_t now_ms);

/*
 * Feed a raw sample (true = pressed). SHORT is reported on release of a
 * press shorter than BTN_LONG_MS; LONG as soon as a hold reaches it.
 */
btn_event_t btn_update(btn_t *b, bool pressed, uint32_t now_ms);

#endif
