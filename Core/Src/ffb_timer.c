#include "ffb_timer.h"
#include "ffb_engine.h"
#include "adc_input.h"
#include "motor.h"
#include "config.h"
#include "failsafe.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_tim.h"

TIM_HandleTypeDef htim2;

/* Velocity calculation state — kept here, not in main.c */
static float  s_prev_angle = 0.0f;
static float  s_velocity   = 0.0f;

/* ── Soft lock ───────────────────────────────────────────────────────────── */
static int32_t apply_soft_lock(int32_t torque, float angle)
{
    float limit  = (float)g_cfg.wheel_range / 2.0f;
    float margin = 8.0f;   /* degrees before hard stop where force ramps   */

    float over_right = angle - (limit - margin);
    float over_left  = -(angle + (limit - margin));

    if (over_right > 0.0f) {
        /* Approaching right limit — scale down torque toward 0 then     */
        /* allow only left (negative) force past hard limit              */
        if (torque > 0) {
            float factor = 1.0f - (over_right / margin);
            if (factor < 0.0f) factor = 0.0f;
            torque = (int32_t)((float)torque * factor);
        }
    } else if (over_left > 0.0f) {
        if (torque < 0) {
            float factor = 1.0f - (over_left / margin);
            if (factor < 0.0f) factor = 0.0f;
            torque = (int32_t)((float)torque * factor);
        }
    }
    return torque;
}

/* ── Init TIM2 at 1 kHz ──────────────────────────────────────────────────── */
void FFB_Timer_Init(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();

    /* APB1 timer clock = 84 MHz on STM32F401 @ 84 MHz
       Prescaler = 83 → 84 MHz / (83+1) = 1 MHz timer clock
       Period    = 999 → 1 MHz / 1000 = 1 kHz interrupt exactly           */
    htim2.Instance               = TIM2;
    htim2.Init.Prescaler         = 83;
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = 999;
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_Base_Init(&htim2);

    /* Enable TIM2 update interrupt — fires every 1 ms precisely.
       Priority 0 = highest, so TIM2 (WDT kick + FFB) can never be starved
       by the USB OTG interrupt. USB is set to priority 1 in main.c.       */
    HAL_NVIC_SetPriority(TIM2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
    HAL_TIM_Base_Start_IT(&htim2);
}

/* ── Called from TIM2 IRQHandler every 1 ms ─────────────────────────────── */
void FFB_Timer_Tick(void)
{
    /* Read current wheel angle (needed for both failsafe and FFB)         */
    float angle = ADC_GetWheelAngle();

    /* Velocity: degrees per second using 1ms fixed dt                     */
    s_velocity = (angle - s_prev_angle) * 1000.0f;
    if (s_velocity >  3600.0f) s_velocity =  3600.0f;
    if (s_velocity < -3600.0f) s_velocity = -3600.0f;
    s_prev_angle = angle;

    /* Calculate FFB torque (0 if no effects active)                       */
    int32_t torque = FFB_Calculate(angle, s_velocity);

    /* Failsafe_Update MUST be called every tick — it kicks the IWDG
       watchdog unconditionally. If we skip it (e.g. early return), the
       WDT fires after 500ms and resets the MCU.                           */
    Failsafe_Update(torque, angle);

    /* If failsafe is triggered, motor is already stopped inside Update()  */
    if (Failsafe_IsTriggered()) {
        return;
    }

    /* Apply soft lock and drive motor */
    torque = apply_soft_lock(torque, angle);
    Motor_Set(torque, g_cfg.max_torque);
}

/* ── TIM2 IRQ Handler — must be in main .c or stm32f4xx_it.c ────────────── */
/* Add this to stm32f4xx_it.c (CubeMX generates that file):
 *
 *   extern void FFB_Timer_Tick(void);
 *
 *   void TIM2_IRQHandler(void)
 *   {
 *       if (__HAL_TIM_GET_FLAG(&htim2, TIM_FLAG_UPDATE) &&
 *           __HAL_TIM_GET_IT_SOURCE(&htim2, TIM_IT_UPDATE))
 *       {
 *           __HAL_TIM_CLEAR_IT(&htim2, TIM_IT_UPDATE);
 *           FFB_Timer_Tick();
 *       }
 *   }
 *
 * NOTE: htim2 in stm32f4xx_it.c must be declared extern from ffb_timer.c
 */

/* ── Also remove from main.c while(1) loop: ─────────────────────────────── */
/* DELETE these lines from while(1) in main.c since timer handles it now:
 *
 *   if ((now - t_ffb) >= 1) {
 *       t_ffb = now;
 *       ... FFB calc and Motor_Set ...
 *   }
 *
 * KEEP in while(1): HID report sending and Serial_SendLive().
 */
