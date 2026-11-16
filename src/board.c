/*
 * board.c - PCB Ornament hardware layer
 * Pin map taken from the Altium PCB (R1..R40 feed D1..D40).
 */
#include "board.h"

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} led_pin_t;

/* Index 0 = D1 ... index 39 = D40, in ring order. */
static const led_pin_t LEDS[LED_COUNT] = {
    {GPIOA, GPIO_PIN_0},  /* D1  */
    {GPIOC, GPIO_PIN_2},  /* D2  */
    {GPIOA, GPIO_PIN_1},  /* D3  */
    {GPIOC, GPIO_PIN_3},  /* D4  */
    {GPIOA, GPIO_PIN_2},  /* D5  */
    {GPIOC, GPIO_PIN_4},  /* D6  */
    {GPIOA, GPIO_PIN_3},  /* D7  */
    {GPIOC, GPIO_PIN_5},  /* D8  */
    {GPIOA, GPIO_PIN_4},  /* D9  */
    {GPIOC, GPIO_PIN_6},  /* D10 */
    {GPIOA, GPIO_PIN_5},  /* D11 */
    {GPIOC, GPIO_PIN_7},  /* D12 */
    {GPIOA, GPIO_PIN_6},  /* D13 */
    {GPIOC, GPIO_PIN_8},  /* D14 */
    {GPIOA, GPIO_PIN_7},  /* D15 */
    {GPIOC, GPIO_PIN_9},  /* D16 */
    {GPIOA, GPIO_PIN_8},  /* D17 */
    {GPIOC, GPIO_PIN_10}, /* D18 */
    {GPIOA, GPIO_PIN_9},  /* D19 */
    {GPIOC, GPIO_PIN_11}, /* D20 */
    {GPIOA, GPIO_PIN_10}, /* D21 */
    {GPIOC, GPIO_PIN_12}, /* D22 */
    {GPIOA, GPIO_PIN_11}, /* D23 */
    {GPIOB, GPIO_PIN_0},  /* D24 */
    {GPIOA, GPIO_PIN_12}, /* D25 */
    {GPIOB, GPIO_PIN_1},  /* D26 */
    {GPIOA, GPIO_PIN_15}, /* D27 - JTDI after reset, freed in board_init */
    {GPIOB, GPIO_PIN_2},  /* D28 - also BOOT1, fine as output */
    {GPIOC, GPIO_PIN_0},  /* D29 */
    {GPIOB, GPIO_PIN_3},  /* D30 - JTDO after reset, freed in board_init */
    {GPIOC, GPIO_PIN_1},  /* D31 */
    {GPIOB, GPIO_PIN_4},  /* D32 - NJTRST after reset, freed in board_init */
    {GPIOB, GPIO_PIN_8},  /* D33 */
    {GPIOB, GPIO_PIN_5},  /* D34 */
    {GPIOB, GPIO_PIN_9},  /* D35 */
    {GPIOB, GPIO_PIN_6},  /* D36 */
    {GPIOB, GPIO_PIN_10}, /* D37 */
    {GPIOB, GPIO_PIN_7},  /* D38 */
    {GPIOB, GPIO_PIN_11}, /* D39 */
    {GPIOB, GPIO_PIN_12}, /* D40 */
};

#define BUTTON_PORT GPIOB
#define BUTTON_PIN  GPIO_PIN_15

static uint64_t led_on_mask; /* bit i set = LED i is on */

/* HAL_Delay()/HAL_GetTick() need the 1 ms SysTick. */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

void board_init(void)
{
    /* Runs from the internal 8 MHz HSI oscillator (no crystal on the board). */
    HAL_Init();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();

    /*
     * PA15, PB3 and PB4 are JTAG pins after reset. Turn JTAG off but keep
     * SWD (SWJ_CFG = 010) so they can drive LEDs.
     * NEVER use __HAL_AFIO_REMAP_SWJ_DISABLE(): it also turns off SWD,
     * and NRST is not wired to the programmer, so you'd be locked out.
     */
    __HAL_AFIO_REMAP_SWJ_NOJTAG();

    /* Drive every LED pin low before making it an output, so nothing flashes. */
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        HAL_GPIO_WritePin(LEDS[i].port, LEDS[i].pin, GPIO_PIN_RESET);
    }

    GPIO_InitTypeDef g = {0};
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        g.Pin = LEDS[i].pin;
        HAL_GPIO_Init(LEDS[i].port, &g);
    }

    /* Button to GND, so use the internal pull-up. */
    g.Pin = BUTTON_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(BUTTON_PORT, &g);

    led_on_mask = 0;
}

uint8_t led_count_on(void)
{
    return (uint8_t)__builtin_popcountll(led_on_mask);
}

bool led_set(uint8_t index, bool on)
{
    if (index >= LED_COUNT) {
        return false;
    }

    const uint64_t bit = 1ULL << index;

    if (on) {
        if (!(led_on_mask & bit) && led_count_on() >= LED_MAX_ON) {
            return false; /* would exceed the current budget */
        }
        led_on_mask |= bit;
        HAL_GPIO_WritePin(LEDS[index].port, LEDS[index].pin, GPIO_PIN_SET);
    } else {
        led_on_mask &= ~bit;
        HAL_GPIO_WritePin(LEDS[index].port, LEDS[index].pin, GPIO_PIN_RESET);
    }
    return true;
}

void led_all_off(void)
{
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        led_set(i, false);
    }
}

bool button_is_pressed(void)
{
    return HAL_GPIO_ReadPin(BUTTON_PORT, BUTTON_PIN) == GPIO_PIN_RESET;
}
