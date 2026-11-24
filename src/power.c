/*
 * power.c - battery measurement, Stop mode, and mode memory (board layer).
 *
 * SWD safety (CLAUDE.md rule 2): nothing here touches AFIO_MAPR or
 * PA13/PA14. PD0/PD1 are left at their reset state because making them
 * GPIO/analog needs an AFIO_MAPR remap.
 */
#include "power.h"
#include "board.h"
#include "mode_store.h"

/* ---- Tweakable ------------------------------------------------------- */
/* Internal reference voltage, mV. Datasheet: 1.16-1.26 V, typ 1.20 V.
 * Adjust if the gauge reads consistently high or low against a meter. */
#define VREFINT_MV 1200U
/* ---------------------------------------------------------------------- */

/* Last 1 KB flash page; platformio.ini caps the firmware below it. */
#define STORE_ADDR  0x0801FC00UL
#define STORE_SLOTS (FLASH_PAGE_SIZE / 2)
#define STORE_PAGE  ((const volatile uint16_t *)STORE_ADDR)

void power_init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Mode = GPIO_MODE_ANALOG;
    g.Pull = GPIO_NOPULL;
    g.Pin = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;  /* PC13-15: never drive (rule 3) */
    HAL_GPIO_Init(GPIOC, &g);
    g.Pin = GPIO_PIN_13 | GPIO_PIN_14;                /* PB13, PB14 */
    HAL_GPIO_Init(GPIOB, &g);
    g.Pin = GPIO_PIN_2;                               /* PD2 */
    HAL_GPIO_Init(GPIOD, &g);
}

uint32_t power_vdd_mv(void)
{
    __HAL_RCC_ADC1_CLK_ENABLE();
    /* ADC clock = PCLK2 / 2 = 4 MHz (max 14 MHz). */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_ADCPRE) | RCC_CFGR_ADCPRE_DIV2;

    ADC1->CR1 = 0;
    ADC1->SQR1 = 0;                          /* one conversion */
    ADC1->SQR3 = 17;                         /* channel 17 = VREFINT */
    ADC1->SMPR1 = 7UL << ((17 - 10) * 3);    /* 239.5 cycles (VREFINT needs >17 us) */
    /* Power on, connect VREFINT, software trigger (EXTSEL = 111). */
    ADC1->CR2 = ADC_CR2_ADON | ADC_CR2_TSVREFE | ADC_CR2_EXTTRIG | ADC_CR2_EXTSEL;
    HAL_Delay(2);                            /* ADC and VREFINT start-up */

    ADC1->CR2 |= ADC_CR2_RSTCAL;
    while (ADC1->CR2 & ADC_CR2_RSTCAL) {}
    ADC1->CR2 |= ADC_CR2_CAL;
    while (ADC1->CR2 & ADC_CR2_CAL) {}

    uint32_t sum = 0;
    for (int k = 0; k < 16; k++) {
        ADC1->CR2 |= ADC_CR2_SWSTART;
        while (!(ADC1->SR & ADC_SR_EOC)) {}
        sum += ADC1->DR & 0xFFFU;
    }

    ADC1->CR2 = 0;                           /* power the ADC back down */
    __HAL_RCC_ADC1_CLK_DISABLE();

    uint32_t raw = sum / 16U;
    return raw ? VREFINT_MV * 4095U / raw : 0;
}

int power_load_mode(uint8_t mode_count)
{
    return store_latest(STORE_PAGE, STORE_SLOTS, mode_count);
}

bool power_save_needs_erase(void)
{
    return store_next_slot(STORE_PAGE, STORE_SLOTS) < 0;
}

void power_save_mode(uint8_t mode)
{
    int slot = store_next_slot(STORE_PAGE, STORE_SLOTS);
    HAL_FLASH_Unlock();
    if (slot < 0) {
        FLASH_EraseInitTypeDef e = {0};
        e.TypeErase = FLASH_TYPEERASE_PAGES;
        e.PageAddress = STORE_ADDR;
        e.NbPages = 1;
        uint32_t bad = 0;
        HAL_FLASHEx_Erase(&e, &bad);
        slot = 0;
    }
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, STORE_ADDR + 2U * (uint32_t)slot,
                      store_record(mode));
    HAL_FLASH_Lock();
}

void EXTI15_10_IRQHandler(void)
{
    EXTI->PR = EXTI_PR_PR15;
}

void power_stop_until_button(void)
{
    /* EXTI15 <- PB15 (EXTICR4 bits 12..15 = 0001). EXTICR, not MAPR. */
    __HAL_RCC_AFIO_CLK_ENABLE();
    AFIO->EXTICR[3] = (AFIO->EXTICR[3] & ~AFIO_EXTICR4_EXTI15) | AFIO_EXTICR4_EXTI15_PB;
    EXTI->RTSR &= ~EXTI_RTSR_TR15;
    EXTI->FTSR |= EXTI_FTSR_TR15;
    EXTI->PR = EXTI_PR_PR15;
    EXTI->IMR |= EXTI_IMR_MR15;
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 2, 0);
    HAL_NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

    /* A debugger may have left "keep clocks in Stop" set; clear it so the
     * chip really drops to microamps. (SWD can't reach it while stopped;
     * ornament.c never stops within 10 s of a reset.) */
    DBGMCU->CR &= ~(DBGMCU_CR_DBG_STOP | DBGMCU_CR_DBG_STANDBY);

    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_SuspendTick();
    HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);

    /*
     * Woken. Stop mode always resumes on the HSI; we never use HSE or the
     * PLL, so the system clock is already back to its normal 8 MHz and
     * there is nothing else to restore.
     */
    HAL_ResumeTick();

    EXTI->IMR &= ~EXTI_IMR_MR15;
    EXTI->PR = EXTI_PR_PR15;
    HAL_NVIC_DisableIRQ(EXTI15_10_IRQn);
}
