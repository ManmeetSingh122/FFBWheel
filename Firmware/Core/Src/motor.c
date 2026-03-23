#include "motor.h"
#include "ffb_wheel.h"
#include "stm32f4xx_hal.h"

/* TIM1 handle - initialised in Motor_Init() */
static TIM_HandleTypeDef htim1;

void Motor_Init(void)
{
    /* ── Enable clocks ─────────────────────────────────────────────────── */
    __HAL_RCC_TIM1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* ── Configure PA8 (CH1) and PA9 (CH2) as TIM1 AF ─────────────────── */
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF1_TIM1;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* ── TIM1 base: 20 kHz, ARR=PWM_MAX (999) ──────────────────────────── */
    /* APB2 timer clock = 84 MHz on STM32F401 @ 84 MHz system clock        */
    /* Prescaler = 3  →  84 MHz / (3+1) = 21 MHz                           */
    /* ARR = 999+1 = 1000 steps  →  21 MHz / 1000 = 21 kHz ≈ 20 kHz       */
    htim1.Instance               = TIM1;
    htim1.Init.Prescaler         = 3;
    htim1.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim1.Init.Period            = PWM_MAX;
    htim1.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim1);

    /* ── Configure CH1 and CH2 as PWM Mode 1 ───────────────────────────── */
    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode       = TIM_OCMODE_PWM1;
    oc.Pulse        = 0;
    oc.OCPolarity   = TIM_OCPOLARITY_HIGH;
    oc.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
    oc.OCFastMode   = TIM_OCFAST_DISABLE;
    oc.OCIdleState  = TIM_OCIDLESTATE_RESET;
    oc.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    HAL_TIM_PWM_ConfigChannel(&htim1, &oc, TIM_CHANNEL_1);
    HAL_TIM_PWM_ConfigChannel(&htim1, &oc, TIM_CHANNEL_2);

    /* ── Start both channels at 0 duty ─────────────────────────────────── */
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);

    /* Enable main output (required for TIM1 advanced timer) */
    __HAL_TIM_MOE_ENABLE(&htim1);
}

void Motor_Set(int32_t value, uint8_t max_pct)
{
    /* Apply safety ceiling */
    int32_t ceiling = (PWM_MAX * max_pct) / 100;

    /* Clamp to ±ceiling */
    if (value >  ceiling) value =  ceiling;
    if (value < -ceiling) value = -ceiling;

    if (value > 0) {
        /* Forward: RPWM active, LPWM = 0 */
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, (uint32_t)value);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 0);
    } else if (value < 0) {
        /* Reverse: LPWM active, RPWM = 0 */
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, (uint32_t)(-value));
    } else {
        Motor_Stop();
    }
}

void Motor_Stop(void)
{
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 0);
}
