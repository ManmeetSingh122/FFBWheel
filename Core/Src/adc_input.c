#include "adc_input.h"
#include "config.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_adc.h"
#include "stm32f4xx_hal_dma.h"

/* ADC DMA result buffer — raw values written by DMA continuously */
volatile uint16_t g_adc[ADC_CHANNELS];

/* ── Filtered ADC values ─────────────────────────────────────────────────── */
/* Two-stage noise rejection:
   Stage 1 — Exponential moving average (EMA):
     alpha = 1/32 (shift by 5). Smooths high-frequency noise.
     Updated every 5ms from the main loop.
   Stage 2 — Hysteresis (deadband):
     Output only updates when the filtered value changes by more than
     ADC_HYSTERESIS counts. Eliminates the last 1-3 count jitter that
     EMA alone can't remove on a noisy DIY board.                           */
#define EMA_SHIFT       5     /* alpha = 1/32                               */
#define ADC_HYSTERESIS  3     /* counts — ignore changes smaller than this  */

static uint32_t s_filtered[ADC_CHANNELS];   /* Q5 fixed-point (value * 32)  */
static uint16_t s_output[ADC_CHANNELS];     /* hysteresis-gated output      */
static uint8_t  s_filter_init = 0;

void ADC_Filter_Update(void)
{
    if (!s_filter_init) {
        for (uint8_t i = 0; i < ADC_CHANNELS; i++) {
            s_filtered[i] = (uint32_t)g_adc[i] << EMA_SHIFT;
            s_output[i]   = g_adc[i];
        }
        s_filter_init = 1;
        return;
    }
    for (uint8_t i = 0; i < ADC_CHANNELS; i++) {
        /* Stage 1: EMA */
        s_filtered[i] = s_filtered[i]
                        - (s_filtered[i] >> EMA_SHIFT)
                        + (uint32_t)g_adc[i];

        /* Stage 2: hysteresis — only update output if change > threshold  */
        uint16_t ema_val = (uint16_t)(s_filtered[i] >> EMA_SHIFT);
        int32_t  delta   = (int32_t)ema_val - (int32_t)s_output[i];
        if (delta < 0) delta = -delta;
        if (delta > ADC_HYSTERESIS)
            s_output[i] = ema_val;
    }
}

static inline uint16_t adc_filtered(uint8_t ch)
{
    return s_output[ch];
}

/* Public version — for Serial_SendLive to report filtered values           */
uint16_t ADC_GetRawFiltered(uint8_t ch)
{
    return (ch < ADC_CHANNELS) ? adc_filtered(ch) : 0;
}

static ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

/* ── Init ────────────────────────────────────────────────────────────────── */
void ADC_Input_Init(void)
{
    /* Enable clocks */
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();   /* needed for PB8 reverse switch        */

    /* Configure PB8 as reverse switch input (INPUT_PULLUP)
       Wire: one side of microswitch to PB8, other side to GND.
       Press knob down + move to 1st gear position = Reverse.               */
    GPIO_InitTypeDef rev_gpio = {0};
    rev_gpio.Pin  = GPIO_PIN_8;
    rev_gpio.Mode = GPIO_MODE_INPUT;
    rev_gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &rev_gpio);

    /* Configure PA0-PA5 as analog input */
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin  = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|
                GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* DMA2 Stream0 Channel0 for ADC1 */
    hdma_adc1.Instance                 = DMA2_Stream0;
    hdma_adc1.Init.Channel             = DMA_CHANNEL_0;
    hdma_adc1.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_adc1.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_adc1.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc1.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
    hdma_adc1.Init.Mode                = DMA_CIRCULAR;
    hdma_adc1.Init.Priority            = DMA_PRIORITY_LOW;
    hdma_adc1.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&hdma_adc1);
    __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1);

    /* ADC1 configuration */
    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCKPRESCALER_PCLK_DIV4;
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode          = ENABLE;
    hadc1.Init.ContinuousConvMode    = ENABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIG_EDGE_NONE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion       = ADC_CHANNELS;
    hadc1.Init.DMAContinuousRequests = ENABLE;
    hadc1.Init.EOCSelection          = EOC_SINGLE_CONV;
    HAL_ADC_Init(&hadc1);

    /* Configure 6 channels: PA0=CH0 .. PA5=CH5 */
    ADC_ChannelConfTypeDef ch = {0};
    /* 480 cycles sampling time — required for high-impedance pot sources
       (10kΩ typical). At 84MHz/4 = 21MHz ADC clock, 480 cycles = ~23µs
       per channel. This fully charges the internal S/H capacitor and
       eliminates inter-channel crosstalk (bleed from adjacent channels).
       84 cycles was too short for 10kΩ sources and caused the throttle
       value to appear on unconnected brake/clutch pins.                    */
    ch.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    for (uint8_t i = 0; i < ADC_CHANNELS; i++) {
        ch.Channel = ADC_CHANNEL_0 + i;     /* ADC_CHANNEL_0 .. ADC_CHANNEL_5 = 0..5 */
        ch.Rank    = i + 1;
        HAL_ADC_ConfigChannel(&hadc1, &ch);
    }

    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

    /* Start DMA circular conversion */
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)g_adc, ADC_CHANNELS);
}

/* ── Raw read (unfiltered — for diagnostics only) ───────────────────────── */
uint16_t ADC_GetRaw(uint8_t ch)
{
    return (ch < ADC_CHANNELS) ? g_adc[ch] : 0;
}

/* ── Apply curve ─────────────────────────────────────────────────────────── */
static int16_t apply_curve(uint16_t raw, uint16_t cal_min, uint16_t cal_max,
                            uint8_t invert, uint8_t curve)
{
    /* Guard against uncalibrated / disconnected pedal */
    if (cal_max <= cal_min) return 0;

    /* Clamp to calibration range */
    if (raw < cal_min) raw = cal_min;
    if (raw > cal_max) raw = cal_max;

    float norm = (float)(raw - cal_min) / (float)(cal_max - cal_min); /* 0-1 */

    if (invert) norm = 1.0f - norm;

    /* Apply curve */
    switch (curve) {
        case 1: norm = norm * norm;                       break; /* square  */
        case 2: norm = norm * norm * norm;                break; /* cubic   */
        default: break;                                           /* linear  */
    }

    /* Map to -32767 to +32767 */
    return (int16_t)((norm * 65534.0f) - 32767.0f);
}

/* ── Axis read ───────────────────────────────────────────────────────────── */
int16_t ADC_GetAxis(uint8_t axis)
{
    uint16_t raw = adc_filtered(axis);   /* use filtered value */
    switch (axis) {
        case ADC_IDX_STEER:
            /* Steering is handled separately via ADC_GetWheelAngle */
            return 0;
        case ADC_IDX_THR:
            return apply_curve(raw, g_cfg.thr_min, g_cfg.thr_max,
                               0, g_cfg.thr_curve);
        case ADC_IDX_BRAKE:
            return apply_curve(raw, g_cfg.brake_min, g_cfg.brake_max,
                               0, g_cfg.brake_curve);
        case ADC_IDX_CLUTCH:
            return apply_curve(raw, g_cfg.clutch_min, g_cfg.clutch_max,
                               0, g_cfg.clutch_curve);
        default:
            return 0;
    }
}

/* ── Steering angle (degrees) ────────────────────────────────────────────── */
float ADC_GetWheelAngle(void)
{
    uint16_t raw = adc_filtered(ADC_IDX_STEER);   /* use filtered value */

    /* Clamp to calibration range */
    uint16_t mn = g_cfg.steer_min;
    uint16_t mx = g_cfg.steer_max;
    uint16_t ct = g_cfg.steer_center;

    if (raw < mn) raw = mn;
    if (raw > mx) raw = mx;

    float pot_angle;

    /* Guard against bad calibration (uncalibrated or miscalibrated) */
    uint16_t range_right = (mx > ct) ? (mx - ct) : 1;
    uint16_t range_left  = (ct > mn) ? (ct - mn) : 1;

    if (raw >= ct) {
        /* Right of centre */
        pot_angle = (float)(raw - ct) / (float)range_right
                    * (g_cfg.pot_degrees / 2.0f);
    } else {
        /* Left of centre */
        pot_angle = -((float)(ct - raw) / (float)range_left)
                    * (g_cfg.pot_degrees / 2.0f);
    }

    /* Scale by pulley ratio to get actual wheel angle */
    float wheel_angle = pot_angle * g_cfg.pulley_ratio;

    /* Apply invert */
    if (g_cfg.steer_invert) wheel_angle = -wheel_angle;

    /* Apply centre deadzone */
    float dz = (float)g_cfg.center_deadzone;
    if (wheel_angle > -dz && wheel_angle < dz) wheel_angle = 0.0f;

    return wheel_angle;
}

/* ── Steering velocity (degrees/sec) — NOT USED (velocity computed in
   ffb_timer.c at a fixed 1 kHz rate for accuracy). Kept for reference.  ── */
#if 0
float ADC_GetWheelVelocity(void)
{
    float angle = ADC_GetWheelAngle();
    uint32_t now = HAL_GetTick();
    uint32_t dt_ms = now - s_prev_tick;

    float velocity = 0.0f;
    if (dt_ms > 0 && dt_ms < 200) {
        velocity = (angle - s_prev_angle) * 1000.0f / (float)dt_ms;
    }

    s_prev_angle = angle;
    s_prev_tick  = now;

    return velocity;
}
#endif

/* ── Gear detection ──────────────────────────────────────────────────────── */
Gear_t ADC_GetGear(void)
{
    uint16_t x = adc_filtered(ADC_IDX_SHFT_X);   /* use filtered values */
    uint16_t y = adc_filtered(ADC_IDX_SHFT_Y);

    uint8_t left   = (x < g_cfg.shft_x_left);
    uint8_t right  = (x > g_cfg.shft_x_right);
    uint8_t center = (!left && !right);
    uint8_t fwd    = (y > g_cfg.shft_y_fwd);
    uint8_t rev    = (y < g_cfg.shft_y_rev);

    /* PB8 reverse pushdown switch — LOW when knob is pressed down         */
    uint8_t rev_sw = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_8) == GPIO_PIN_RESET);

    if (center && !fwd && !rev) return GEAR_N;

    /* 1st gear position (left-forward):
       - Normal:       return GEAR_1
       - PB8 pressed:  return GEAR_R  (push knob down to engage reverse)   */
    if (fwd && left)   return rev_sw ? GEAR_R : GEAR_1;

    if (rev  && left)   return GEAR_2;
    if (fwd  && center) return GEAR_3;
    if (rev  && center) return GEAR_4;
    if (fwd  && right)  return GEAR_5;
    if (rev  && right)  return GEAR_6;

    return GEAR_N;
}

DMA_HandleTypeDef* ADC_GetDMAHandle(void)
{
    return &hdma_adc1;
}
