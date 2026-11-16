/*
 * led_bcm.c - BCM brightness engine on TIM2 (board layer).
 *
 * Timing at the 8 MHz HSI (TIM2 clock = 8 MHz, PSC = 0):
 *   LSB slice = 128 ticks = 16 us, plane b lasts 128 << b ticks.
 *   Frame = 255 * 128 = 32640 ticks -> 245 Hz refresh.
 *   8 interrupts per frame (~2000/s), each well under the 128-cycle
 *   shortest slice, so ~1.5 % CPU. No PLL needed, which keeps the MCU
 *   at ~5 mA instead of ~25-30 mA at 64 MHz.
 *
 * ARR is preloaded (ARPE = 1): the ISR that starts slice k writes the
 * length of slice k+1, so slice lengths are set by hardware and ISR
 * latency only shifts every edge by the same amount.
 *
 * GPIO writes bypass led_set() but keep the same budget: every plane was
 * checked by bcm_build(), and the ISR clears all three ports before
 * setting any, so no transient state has more LEDs on than a valid plane.
 */
#include "led_bcm.h"
#include "board.h"
#include <string.h>

_Static_assert(BCM_LED_COUNT == LED_COUNT, "LED count mismatch");
_Static_assert(BCM_MAX_ON == LED_MAX_ON, "current limit mismatch");

#define BCM_LSB_TICKS 128U

static bcm_frame_t frames[2];
static volatile uint8_t front;    /* frame being scanned */
static volatile uint8_t pending;  /* back frame ready, swap at next frame */
static volatile uint8_t running;
static uint8_t plane;             /* plane the next interrupt outputs */

void TIM2_IRQHandler(void)
{
    TIM2->SR = ~TIM_SR_UIF;

    const bcm_plane_t *p = &frames[front].plane[plane];

    /* All clears first, then sets (see header comment). */
    GPIOA->BRR = p->clr[BCM_PORT_A];
    GPIOB->BRR = p->clr[BCM_PORT_B];
    GPIOC->BRR = p->clr[BCM_PORT_C];
    GPIOA->BSRR = p->set[BCM_PORT_A];
    GPIOB->BSRR = p->set[BCM_PORT_B];
    GPIOC->BSRR = p->set[BCM_PORT_C];

    uint8_t next = (uint8_t)((plane + 1) & (BCM_BITS - 1));
    TIM2->ARR = (BCM_LSB_TICKS << next) - 1U; /* length of the next slice */
    plane = next;

    if (next == 0 && pending) {
        front ^= 1U; /* only between frames: never a torn frame */
        pending = 0;
    }
}

void bcm_start(void)
{
    if (running) {
        return;
    }
    led_all_off();
    memset(frames, 0, sizeof(frames));
    for (int b = 0; b < BCM_BITS; b++) {
        for (int p = 0; p < BCM_PORTS; p++) {
            frames[0].plane[b].clr[p] = bcm_port_led_mask(p);
            frames[1].plane[b].clr[p] = bcm_port_led_mask(p);
        }
    }
    front = 0;
    pending = 0;
    plane = 0;

    /*
     * Keep the debug port clocked while the CPU sleeps in WFI, so SWD can
     * always connect (NRST isn't wired to the programmer).
     */
    DBGMCU->CR |= DBGMCU_CR_DBG_SLEEP;

    __HAL_RCC_TIM2_CLK_ENABLE();
    TIM2->CR1 = TIM_CR1_ARPE | TIM_CR1_URS;
    TIM2->PSC = 0;
    TIM2->ARR = BCM_LSB_TICKS - 1U;
    TIM2->EGR = TIM_EGR_UG;  /* load PSC/ARR shadows */
    TIM2->SR = 0;
    TIM2->ARR = BCM_LSB_TICKS - 1U; /* preload: slice 0 = 1 LSB */
    TIM2->DIER = TIM_DIER_UIE;

    /* Highest priority, above SysTick (15), so slice edges don't jitter. */
    HAL_NVIC_SetPriority(TIM2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);

    running = 1;
    TIM2->CR1 |= TIM_CR1_CEN;
}

void bcm_stop(void)
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

uint8_t bcm_submit(const uint16_t duty[BCM_LED_COUNT])
{
    if (!running) {
        return bcm_build(duty, BCM_CAP_Q8, &frames[front]);
    }
    while (pending) {
        __WFI();
    }
    uint8_t cap = bcm_build(duty, BCM_CAP_Q8, &frames[front ^ 1U]);
    pending = 1;
    return cap;
}

void bcm_idle(void)
{
    __WFI();
}
