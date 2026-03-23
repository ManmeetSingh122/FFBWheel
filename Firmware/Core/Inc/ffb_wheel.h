#ifndef FFB_WHEEL_H
#define FFB_WHEEL_H

#include <stdint.h>
#include <string.h>

/* ── Hardware ─────────────────────────────────────────────────────────────── */
/* BTS7960 PWM – TIM1 CH1 (PA8) = RPWM,  TIM1 CH2 (PA9) = LPWM            */
/* R_EN / L_EN : tie both to 3.3V permanently on the BTS7960 board          */
/* USB : PA11 = D-,  PA12 = D+  (built-in USB_OTG_FS on Black Pill)         */
/* ADC : PA0 Steering | PA1 Throttle | PA2 Brake | PA3 Clutch               */
/*        PA4 Shifter-X | PA5 Shifter-Y                                       */
/* Buttons : PB0–PB7  (INPUT_PULLUP – connect switch between pin and GND)   */
/* Reverse  : PB8     (INPUT_PULLUP – microswitch under shifter knob)        */
/*            Push knob DOWN + move to 1st gear position = Reverse           */

/* ── Firmware version ────────────────────────────────────────────────────── */
#define FW_VERSION_MAJOR  1
#define FW_VERSION_MINOR  0

/* ── Wheel parameters (overridden by flash config) ───────────────────────── */
#define DEFAULT_WHEEL_RANGE     900      /* lock-to-lock degrees             */
#define DEFAULT_PULLEY_RATIO    4.5f     /* wheel-degrees / pot-degrees      */
#define DEFAULT_POT_DEGREES     200.0f   /* physical pot sweep               */
#define DEFAULT_FFB_STRENGTH    80       /* 0-100 %                          */
#define DEFAULT_SPRING_GAIN     65       /* 0-100 %                          */
#define DEFAULT_DAMPER_GAIN     40       /* 0-100 %                          */
#define DEFAULT_FRICTION_GAIN   25       /* 0-100 %                          */
#define DEFAULT_INERTIA_GAIN    20       /* 0-100 %                          */
#define DEFAULT_MAX_TORQUE      90       /* 0-100 % – safety ceiling         */
#define DEFAULT_CENTER_DEADZONE 3        /* degrees either side of centre    */

/* ── ADC ─────────────────────────────────────────────────────────────────── */
#define ADC_CHANNELS    6
#define ADC_MAX         4095

/* ADC DMA buffer indices */
#define ADC_IDX_STEER   0
#define ADC_IDX_THR     1
#define ADC_IDX_BRAKE   2
#define ADC_IDX_CLUTCH  3
#define ADC_IDX_SHFT_X  4
#define ADC_IDX_SHFT_Y  5

/* ── PWM ─────────────────────────────────────────────────────────────────── */
#define PWM_FREQ_HZ     20000
#define PWM_MAX         999      /* ARR value for 20 kHz at 20 MHz TIM1 clk */

/* ── Flash ───────────────────────────────────────────────────────────────── */
/* STM32F401CCU6: 256 KB flash – last sector (sector 7) @ 0x08060000, 128KB */
#define CONFIG_FLASH_ADDR   0x08060000UL
#define CONFIG_MAGIC        0xFFBB0101UL  /* changes when struct layout changes */

/* ── HID axis range ──────────────────────────────────────────────────────── */
#define HID_AXIS_MIN   -32767
#define HID_AXIS_MAX    32767

/* ── Gear definitions ────────────────────────────────────────────────────── */
typedef enum { GEAR_N=0, GEAR_1, GEAR_2, GEAR_3,
               GEAR_4, GEAR_5, GEAR_6, GEAR_R } Gear_t;

/* ── Config struct (saved to flash) ─────────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint32_t magic;

    /* Wheel */
    uint16_t wheel_range;       /* 180 – 1800 degrees                       */
    float    pulley_ratio;      /* e.g. 4.5                                  */
    float    pot_degrees;       /* physical pot sweep, e.g. 200.0            */
    uint8_t  steer_invert;      /* 0 or 1                                   */
    uint16_t steer_center;      /* ADC value at wheel centre                */
    uint16_t steer_min;         /* ADC value at full left lock              */
    uint16_t steer_max;         /* ADC value at full right lock             */

    /* FFB */
    uint8_t  ffb_strength;      /* 0-100                                    */
    uint8_t  spring_gain;
    uint8_t  damper_gain;
    uint8_t  friction_gain;
    uint8_t  inertia_gain;
    uint8_t  max_torque;

    /* Wheel misc */
    uint8_t  center_deadzone;   /* degrees                                  */

    /* Pedals */
    uint16_t thr_min,   thr_max;
    uint16_t brake_min, brake_max;
    uint16_t clutch_min,clutch_max;
    uint8_t  thr_curve;         /* 0=linear 1=square                        */
    uint8_t  brake_curve;
    uint8_t  clutch_curve;

    /* Shifter */
    uint16_t shft_x_left;       /* ADC threshold: left gate                 */
    uint16_t shft_x_right;      /* ADC threshold: right gate                */
    uint16_t shft_y_fwd;        /* ADC threshold: forward                   */
    uint16_t shft_y_rev;        /* ADC threshold: reverse                   */

    uint8_t  padding[8];        /* reserved                                 */
} WheelConfig_t;

/* Global config instance (defined in config.c) */
extern WheelConfig_t g_cfg;

/* Global ADC DMA buffer */
extern volatile uint16_t g_adc[ADC_CHANNELS];

#endif /* FFB_WHEEL_H */
