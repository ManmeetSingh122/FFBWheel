#include "adc_input.h"
#include "config.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_adc.h"
#include "stm32f4xx_hal_dma.h"

/* ADC DMA result buffer */
volatile uint16_t g_adc[ADC_CHANNELS];

/* ── Two-stage filter ────────────────────────────────────────────────────── */
/* Stage 1: EMA with configurable shift (alpha = 1/2^shift)
   Stage 2: Hysteresis — output only updates when change > threshold        */
#define ADC_HYSTERESIS  3

static uint32_t s_filtered[ADC_CHANNELS];
static uint16_t s_output[ADC_CHANNELS];
static uint8_t  s_filter_init = 0;

/* Map filter_strength (0-3) to EMA shift (0=no filter, 3=heavy)           */
static uint8_t get_ema_shift(void)
{
    switch (g_cfg.filter_strength) {
        case 0:  return 0;   /* off — pass through raw                      */
        case 1:  return 3;   /* light  — alpha = 1/8,  ~8ms lag             */
        case 3:  return 6;   /* heavy  — alpha = 1/64, ~64ms lag            */
        default: return 5;   /* medium — alpha = 1/32, ~32ms lag (default)  */
    }
}

void ADC_Filter_Update(void)
{
    uint8_t shift = get_ema_shift();

    if (!s_filter_init) {
        for (uint8_t i = 0; i < ADC_CHANNELS; i++) {
            s_filtered[i] = (uint32_t)g_adc[i] << (shift ? shift : 1);
            s_output[i]   = g_adc[i];
        }
        s_filter_init = 1;
        return;
    }

    for (uint8_t i = 0; i < ADC_CHANNELS; i++) {
        uint32_t raw = (uint32_t)g_adc[i];

        if (shift == 0) {
            /* Filter off — use raw directly, still apply hysteresis       */
            s_filtered[i] = raw << 1;
        } else {
            s_filtered[i] = s_filtered[i]
                            - (s_filtered[i] >> shift)
                            + raw;
        }

        uint16_t ema_val = (uint16_t)(s_filtered[i] >> (shift ? shift : 1));
        int32_t  delta   = (int32_t)ema_val - (int32_t)s_output[i];
        if (delta < 0) delta = -delta;
        if (delta > ADC_HYSTERESIS || shift == 0)
            s_output[i] = ema_val;
    }
}

static inline uint16_t adc_filtered(uint8_t ch)
{
    return s_output[ch];
}

uint16_t ADC_GetRawFiltered(uint8_t ch)
{
    return (ch < ADC_CHANNELS) ? adc_filtered(ch) : 0;
}

static ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

/* ── Init ────────────────────────────────────────────────────────────────── */
void ADC_Input_Init(void)
{
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* PB8 — reverse gear switch */
    GPIO_InitTypeDef rev_gpio = {0};
    rev_gpio.Pin  = GPIO_PIN_8;
    rev_gpio.Mode = GPIO_MODE_INPUT;
    rev_gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &rev_gpio);

    /* PA0-PA5 — analog inputs */
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin  = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|
                GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* DMA2 Stream0 Channel0 */
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

    /* ADC1 */
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

    /* 480 cycles — required for 10kΩ pot sources to avoid inter-channel
       crosstalk. Fully charges the S/H capacitor before each conversion.  */
    ADC_ChannelConfTypeDef ch = {0};
    ch.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    for (uint8_t i = 0; i < ADC_CHANNELS; i++) {
        ch.Channel = ADC_CHANNEL_0 + i;
        ch.Rank    = i + 1;
        HAL_ADC_ConfigChannel(&hadc1, &ch);
    }

    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)g_adc, ADC_CHANNELS);
}

uint16_t ADC_GetRaw(uint8_t ch)
{
    return (ch < ADC_CHANNELS) ? g_adc[ch] : 0;
}

/* ── 5-point custom curve interpolation ─────────────────────────────────── */
/* pts[5]: output% at input 0,25,50,75,100%.
   Linearly interpolates between the 5 control points.                      */
static float apply_custom_curve(float norm, const uint8_t *pts)
{
    /* norm is 0.0-1.0, map to segment index 0-3                           */
    float scaled = norm * 4.0f;   /* 4 segments between 5 points           */
    int   seg    = (int)scaled;
    if (seg >= 4) return (float)pts[4] / 100.0f;
    if (seg <  0) return (float)pts[0] / 100.0f;

    float t  = scaled - (float)seg;
    float y0 = (float)pts[seg]     / 100.0f;
    float y1 = (float)pts[seg + 1] / 100.0f;
    return y0 + (y1 - y0) * t;
}

/* ── Pedal axis processing ───────────────────────────────────────────────── */
static int16_t apply_pedal_curve(uint16_t raw, uint16_t cal_min,
                                  uint16_t cal_max, const uint8_t *pts,
                                  uint8_t curve_mode)
{
    if (cal_max <= cal_min) return 0;

    if (raw < cal_min) raw = cal_min;
    if (raw > cal_max) raw = cal_max;

    float norm = (float)(raw - cal_min) / (float)(cal_max - cal_min);

    switch (curve_mode) {
        case 0: norm = apply_custom_curve(norm, pts); break; /* custom pts  */
        case 2: norm = norm * norm;                   break; /* square      */
        case 3: norm = norm * norm * norm;            break; /* cubic       */
        default: break;                                       /* linear      */
    }

    return (int16_t)(norm * 32767.0f);
}

int16_t ADC_GetAxis(uint8_t axis)
{
    uint16_t raw = adc_filtered(axis);
    switch (axis) {
        case ADC_IDX_STEER:  return 0;
        case ADC_IDX_THR:
            return apply_pedal_curve(raw, g_cfg.thr_min, g_cfg.thr_max,
                                     g_cfg.thr_pts, g_cfg.thr_curve);
        case ADC_IDX_BRAKE:
            return apply_pedal_curve(raw, g_cfg.brake_min, g_cfg.brake_max,
                                     g_cfg.brake_pts, g_cfg.brake_curve);
        case ADC_IDX_CLUTCH:
            return apply_pedal_curve(raw, g_cfg.clutch_min, g_cfg.clutch_max,
                                     g_cfg.clutch_pts, g_cfg.clutch_curve);
        default: return 0;
    }
}

/* ── Steering angle (degrees) ────────────────────────────────────────────── */
float ADC_GetWheelAngle(void)
{
    uint16_t raw = adc_filtered(ADC_IDX_STEER);

    uint16_t mn = g_cfg.steer_min;
    uint16_t mx = g_cfg.steer_max;
    uint16_t ct = g_cfg.steer_center;

    if (raw < mn) raw = mn;
    if (raw > mx) raw = mx;

    uint16_t range_right = (mx > ct) ? (mx - ct) : 1;
    uint16_t range_left  = (ct > mn) ? (ct - mn) : 1;

    float pot_angle;
    if (raw >= ct) {
        pot_angle = (float)(raw - ct) / (float)range_right
                    * (g_cfg.pot_degrees / 2.0f);
    } else {
        pot_angle = -((float)(ct - raw) / (float)range_left)
                    * (g_cfg.pot_degrees / 2.0f);
    }

    float wheel_angle = pot_angle * g_cfg.pulley_ratio;

    if (g_cfg.steer_invert) wheel_angle = -wheel_angle;

    float dz = (float)g_cfg.center_deadzone;
    if (wheel_angle > -dz && wheel_angle < dz) wheel_angle = 0.0f;

    return wheel_angle;
}

/* ── Gear detection ──────────────────────────────────────────────────────── */
Gear_t ADC_GetGear(void)
{
    uint16_t x = adc_filtered(ADC_IDX_SHFT_X);
    uint16_t y = adc_filtered(ADC_IDX_SHFT_Y);

    uint8_t left   = (x < g_cfg.shft_x_left);
    uint8_t right  = (x > g_cfg.shft_x_right);
    uint8_t center = (!left && !right);
    uint8_t fwd    = (y > g_cfg.shft_y_fwd);
    uint8_t rev    = (y < g_cfg.shft_y_rev);
    uint8_t rev_sw = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_8) == GPIO_PIN_RESET);

    if (center && !fwd && !rev) return GEAR_N;
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
