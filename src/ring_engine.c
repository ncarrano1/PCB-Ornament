/*
 * ring_engine.c - banked BCM refresh on TIM2 (board layer).
 *
 * Same timing scheme as led_bcm.c (wave), with 14 slices instead of 8:
 *   LSB = 128 ticks = 16 us at 8 MHz; bit-plane b lasts 128 << b ticks.
 *   Frame = 2 banks x 127 LSB x 128 = 32512 ticks -> 246 Hz.
 *   Shortest slice (128 cycles) is longer than the ISR (~85 cycles).
 *
 * Current limit (rule 1): each slice only sets pins of one 20-LED bank
 * (ring_build), and the ISR writes every port's clears before any sets,
 * so even mid-ISR the high pins are a subset of a single bank.
 *
 * ARR is preloaded: the ISR that starts slice k writes the length of
 * slice k+1, so slice lengths are exact regardless of ISR latency.
 */
#include "ring_engine.h"
#include "board.h"

_Static_assert(RING_LEDS == LED_COUNT, "LED count mismatch");
_Static_assert(RING_MAX_ON == LED_MAX_ON, "current limit mismatch");

#define LSB_TICKS 128U

/* ARR value for each slice: (LSB << bit) - 1. */
#define ARR_OF(s) ((uint16_t)((LSB_TICKS << (RING_BITS - 1 - ((s) >> 1))) - 1U))
static const uint16_t SLICE_ARR[RING_SLICES] = {
    ARR_OF(0), ARR_OF(1), ARR_OF(2),  ARR_OF(3),  ARR_OF(4),  ARR_OF(5),  ARR_OF(6),
    ARR_OF(7), ARR_OF(8), ARR_OF(9),  ARR_OF(10), ARR_OF(11), ARR_OF(12), ARR_OF(13),
};
_Static_assert(RING_SLICES == 14, "update SLICE_ARR");

static ring_frame_t frames[2];
static volatile uint8_t front;
static volatile uint8_t pending;
static volatile uint8_t running;
static uint8_t slice;

volatile uint32_t g_build_cycles_max;

void TIM2_IRQHandler(void)
{
    TIM2->SR = ~TIM_SR_UIF;

    const ring_slice_t *p = &frames[front].slice[slice];

    /* All clears first, then sets: no transient mixes two banks. */
    GPIOA->BRR = p->clr[RING_PORT_A];
    GPIOB->BRR = p->clr[RING_PORT_B];
    GPIOC->BRR = p->clr[RING_PORT_C];
    GPIOA->BSRR = p->set[RING_PORT_A];
    GPIOB->BSRR = p->set[RING_PORT_B];
    GPIOC->BSRR = p->set[RING_PORT_C];

    uint8_t next = (uint8_t)(slice + 1U);
    if (next == RING_SLICES) {
        next = 0;
    }
    TIM2->ARR = SLICE_ARR[next];
    slice = next;

    if (next == 0 && pending) {
        front ^= 1U;
        pending = 0;
    }
}

static void clear_frame(ring_frame_t *f)
{
    for (int s = 0; s < RING_SLICES; s++) {
        for (int p = 0; p < RING_PORTS; p++) {
            f->slice[s].set[p] = 0;
            f->slice[s].clr[p] = ring_port_mask(p);
        }
    }
}

uint32_t ring_engine_cycles(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    return DWT->CYCCNT;
}

void ring_engine_start(void)
{
    if (running) {
        return;
    }
    led_all_off();
    clear_frame(&frames[0]);
    clear_frame(&frames[1]);
    front = 0;
    pending = 0;
    slice = 0;

    /* Keep SWD reachable while the CPU sleeps in WFI (NRST isn't wired). */
    DBGMCU->CR |= DBGMCU_CR_DBG_SLEEP;

    __HAL_RCC_TIM2_CLK_ENABLE();
    TIM2->CR1 = TIM_CR1_ARPE | TIM_CR1_URS;
    TIM2->PSC = 0;
    TIM2->ARR = SLICE_ARR[0];
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0;
    TIM2->ARR = SLICE_ARR[0]; /* preload: first slice */
    TIM2->DIER = TIM_DIER_UIE;

    /* Above SysTick (15), so slice edges don't jitter. */
    HAL_NVIC_SetPriority(TIM2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);

    running = 1;
    TIM2->CR1 |= TIM_CR1_CEN;
}

void ring_engine_stop(void)
{
    if (!running) {
        return;
    }
    TIM2->CR1 = 0;
    TIM2->DIER = 0;
    HAL_NVIC_DisableIRQ(TIM2_IRQn);
    TIM2->SR = 0;
    HAL_NVIC_ClearPendingIRQ(TIM2_IRQn);
    __HAL_RCC_TIM2_CLK_DISABLE();
    running = 0;
    pending = 0;
    led_all_off();
}

void ring_engine_submit(const uint8_t bright[RING_LEDS])
{
    if (!running) {
        return;
    }
    while (pending) {
        __WFI();
    }
    uint32_t t0 = ring_engine_cycles();
    uint8_t code[RING_LEDS];
    ring_codes(bright, code);
    ring_build(code, &frames[front ^ 1U]);
    uint32_t dt = ring_engine_cycles() - t0;
    if (dt > g_build_cycles_max) {
        g_build_cycles_max = dt;
    }
    pending = 1;
}

void ring_engine_idle(void)
{
    __WFI();
}
