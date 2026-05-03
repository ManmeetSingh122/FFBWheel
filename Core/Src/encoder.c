#include "encoder.h"
#include "config.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_tim.h"

static TIM_HandleTypeDef htim3;
static int32_t s_center_count = 0;   /* counter value at wheel center       */
static float   s_deg_per_count = 0.0f;

/* ── Init TIM3 in quadrature encoder mode ───────────────────────────────── */
void Encoder_Init(uint16_t ppr)
{
    /* degrees per count: full rotation = 360°, encoder gives ppr*4 counts
       (TIM_ENCODERMODE_TI12 counts both edges of both channels = 4x)      */
    if (ppr == 0) ppr = 600;
    s_deg_per_count = 360.0f / (float)((uint32_t)ppr * 4U);

    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* PB4 = TIM3_CH1, PB5 = TIM3_CH2 — alternate function AF2             */
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin       = GPIO_PIN_4 | GPIO_PIN_5;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_PULLUP;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOB, &gpio);

    /* TIM3 base — period = 0xFFFF (max 16-bit), prescaler = 0             */
    htim3.Instance               = TIM3;
    htim3.Init.Prescaler         = 0;
    htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3.Init.Period            = 0xFFFF;
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    TIM_Encoder_InitTypeDef enc_cfg = {0};
    enc_cfg.EncoderMode  = TIM_ENCODERMODE_TI12;
    enc_cfg.IC1Polarity  = TIM_ICPOLARITY_RISING;
    enc_cfg.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    enc_cfg.IC1Prescaler = TIM_ICPSC_DIV1;
    enc_cfg.IC1Filter    = 4;
    enc_cfg.IC2Polarity  = TIM_ICPOLARITY_RISING;
    enc_cfg.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    enc_cfg.IC2Prescaler = TIM_ICPSC_DIV1;
    enc_cfg.IC2Filter    = 4;
    HAL_TIM_Encoder_Init(&htim3, &enc_cfg);

    /* Start at mid-range so we have room to count both directions          */
    __HAL_TIM_SET_COUNTER(&htim3, 0x8000);
    s_center_count = 0x8000;

    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
}

/* ── Set current position as center ─────────────────────────────────────── */
void Encoder_SetCenter(void)
{
    s_center_count = (int32_t)__HAL_TIM_GET_COUNTER(&htim3);
}

/* ── Get signed count relative to center ────────────────────────────────── */
int32_t Encoder_GetCount(void)
{
    int32_t raw = (int32_t)__HAL_TIM_GET_COUNTER(&htim3);
    return raw - s_center_count;
}

/* ── Get wheel angle in degrees ─────────────────────────────────────────── */
float Encoder_GetAngle(void)
{
    float angle = (float)Encoder_GetCount() * s_deg_per_count;

    /* Apply invert */
    if (g_cfg.steer_invert) angle = -angle;

    /* Apply center deadzone */
    float dz = (float)g_cfg.center_deadzone;
    if (angle > -dz && angle < dz) angle = 0.0f;

    /* Clamp to wheel range */
    float limit = (float)g_cfg.wheel_range / 2.0f;
    if (angle >  limit) angle =  limit;
    if (angle < -limit) angle = -limit;

    return angle;
}
