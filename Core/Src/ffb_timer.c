#include "ffb_timer.h"
#include "ffb_engine.h"
#include "adc_input.h"
#include "encoder.h"
#include "motor.h"
#include "config.h"
#include "failsafe.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_tim.h"
#include <math.h>

TIM_HandleTypeDef htim2;

static float  s_prev_angle   = 0.0f;
static float  s_velocity     = 0.0f;
static float  s_velocity_raw = 0.0f;
static int32_t s_torque_out  = 0;    /* smoothed output torque              */

/* ── Get wheel angle from active input source ────────────────────────────── */
static inline float get_angle(void)
{
    return (g_cfg.input_mode == INPUT_MODE_ENCODER)
           ? Encoder_GetAngle()
           : ADC_GetWheelAngle();
}

/* ── Soft lock (cosine ramp) ─────────────────────────────────────────────── */
/* Ramps down FFB torque in the 8° zone before the hard stop, then applies
   an active bumpstop force pushing the wheel back toward center.           */
static int32_t apply_soft_lock(int32_t torque, float angle)
{
    float limit  = (float)g_cfg.wheel_range / 2.0f;
    float margin = 8.0f;

    float over_right = angle - (limit - margin);
    float over_left  = -(angle + (limit - margin));

    if (over_right > 0.0f) {
        /* Ramp down outward (rightward) force */
        if (torque > 0) {
            float t = over_right / margin;
            if (t > 1.0f) t = 1.0f;
            float factor = 0.5f * (1.0f + cosf(t * 3.14159f));
            torque = (int32_t)((float)torque * factor);
        }
        /* Add active bumpstop: push left (negative) proportional to overshoot */
        if (g_cfg.bumpstop_strength > 0) {
            float bump_t = over_right / margin;
            if (bump_t > 1.0f) bump_t = 1.0f;
            int32_t bump = -(int32_t)(bump_t * (float)g_cfg.bumpstop_strength * 10.0f);
            torque += bump;
        }
    } else if (over_left > 0.0f) {
        if (torque < 0) {
            float t = over_left / margin;
            if (t > 1.0f) t = 1.0f;
            float factor = 0.5f * (1.0f + cosf(t * 3.14159f));
            torque = (int32_t)((float)torque * factor);
        }
        if (g_cfg.bumpstop_strength > 0) {
            float bump_t = over_left / margin;
            if (bump_t > 1.0f) bump_t = 1.0f;
            int32_t bump = (int32_t)(bump_t * (float)g_cfg.bumpstop_strength * 10.0f);
            torque += bump;
        }
    }
    return torque;
}

/* ── Always-on center spring ─────────────────────────────────────────────── */
/* Independent of game FFB. Keeps wheel centered when no game is running.
   Strength 0 = off, 100 = strong. Force is proportional to angle.         */
static int32_t apply_center_spring(int32_t torque, float angle)
{
    if (g_cfg.center_spring == 0) return torque;

    float limit = (float)g_cfg.wheel_range / 2.0f;
    if (limit < 1.0f) limit = 1.0f;

    /* Spring force: proportional to displacement, max ±500 at full lock   */
    float spring = -(angle / limit) * (float)g_cfg.center_spring * 5.0f;
    return torque + (int32_t)spring;
}

/* ── Output torque smoothing ─────────────────────────────────────────────── */
/* Low-pass filter on the final motor output. Reduces mechanical noise and
   prevents sudden torque jumps from causing vibration.
   torque_smooth 0=off, 100=heavy (alpha goes from 1.0 to 0.05)            */
static int32_t apply_torque_smooth(int32_t torque)
{
    if (g_cfg.torque_smooth == 0) {
        s_torque_out = torque;
        return torque;
    }
    /* alpha = 1.0 - (smooth/100 * 0.95) → range 0.05 to 1.0              */
    float alpha = 1.0f - ((float)g_cfg.torque_smooth / 100.0f * 0.95f);
    s_torque_out = (int32_t)((float)s_torque_out * (1.0f - alpha)
                             + (float)torque * alpha);
    return s_torque_out;
}

/* ── Init TIM2 at 1 kHz ──────────────────────────────────────────────────── */
void FFB_Timer_Init(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();

    /* APB1 = 84 MHz, prescaler 83 → 1 MHz tick, period 999 → 1 kHz IRQ  */
    htim2.Instance               = TIM2;
    htim2.Init.Prescaler         = 83;
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = 999;
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_Base_Init(&htim2);

    /* Priority 0 — highest. USB is priority 5. WDT kick must never be
       starved by USB interrupt activity.                                   */
    HAL_NVIC_SetPriority(TIM2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
    HAL_TIM_Base_Start_IT(&htim2);
}

/* ── Called from TIM2 IRQHandler every 1 ms ─────────────────────────────── */
void FFB_Timer_Tick(void)
{
    float angle = get_angle();

    /* Velocity: deg/s, clamped to ±3600 (10 rev/s max — beyond is noise).
       Low-pass filtered to smooth damper/inertia and prevent motor buzz.  */
    s_velocity_raw = (angle - s_prev_angle) * 1000.0f;
    if (s_velocity_raw >  3600.0f) s_velocity_raw =  3600.0f;
    if (s_velocity_raw < -3600.0f) s_velocity_raw = -3600.0f;
    s_velocity = s_velocity * 0.9f + s_velocity_raw * 0.1f;
    s_prev_angle = angle;

    /* Game FFB torque */
    int32_t torque = FFB_Calculate(angle, s_velocity);

    /* Always-on center spring (independent of game FFB)                   */
    torque = apply_center_spring(torque, angle);

    /* Failsafe — kicks WDT unconditionally, must be called every tick     */
    Failsafe_Update(torque, angle);

    if (Failsafe_IsTriggered()) {
        s_torque_out = 0;
        return;
    }

    /* Soft lock + bumpstop */
    torque = apply_soft_lock(torque, angle);

    /* Output smoothing */
    torque = apply_torque_smooth(torque);

    Motor_Set(torque, g_cfg.max_torque);
}
