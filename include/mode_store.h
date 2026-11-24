/*
 * mode_store.h - append-only log of the selected mode in one flash page.
 * Pure C: works on a copy/view of the page so it can be tested on the host.
 *
 * Each record is one 16-bit halfword: 0xA5 << 8 | mode. New records go in
 * the first erased (0xFFFF) slot; the page is erased only when full, so a
 * 1 KB page takes 512 saves per erase cycle.
 */
#ifndef MODE_STORE_H
#define MODE_STORE_H

#include <stdint.h>

#define STORE_MAGIC 0xA5U
#define STORE_EMPTY 0xFFFFU

static inline uint16_t store_record(uint8_t mode)
{
    return (uint16_t)(STORE_MAGIC << 8 | mode);
}

/* Mode in the newest record, or -1 if there is none or it is invalid. */
int store_latest(const volatile uint16_t *page, int slots, uint8_t mode_count);

/* Slot for the next record, or -1 if the page is full (erase first). */
int store_next_slot(const volatile uint16_t *page, int slots);

#endif
