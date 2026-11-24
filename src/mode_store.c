/*
 * mode_store.c - append-only mode log in one flash page. Pure C.
 */
#include "mode_store.h"

/* Index of the last slot that has been written, or -1. */
static int last_written(const volatile uint16_t *page, int slots)
{
    for (int i = slots - 1; i >= 0; i--) {
        if (page[i] != STORE_EMPTY) {
            return i;
        }
    }
    return -1;
}

int store_latest(const volatile uint16_t *page, int slots, uint8_t mode_count)
{
    /* Only the newest record counts; if it is invalid (garbage, a write cut
     * short by power loss, or a mode this firmware doesn't have), the
     * caller falls back to mode 1. */
    int i = last_written(page, slots);
    if (i < 0) {
        return -1;
    }
    uint16_t r = page[i];
    if ((r >> 8) == STORE_MAGIC && (r & 0xFFU) < mode_count) {
        return (int)(r & 0xFFU);
    }
    return -1;
}

int store_next_slot(const volatile uint16_t *page, int slots)
{
    int i = last_written(page, slots) + 1;
    return i < slots ? i : -1;
}
