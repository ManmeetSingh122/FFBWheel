#ifndef FFB_WHEEL_H
#define FFB_WHEEL_H

#include <stdint.h>
#include <string.h>

/* ── Hardware ─────────────────────────────────────────────────────────────── */
/* BTS7960 PWM  – TIM1 CH1 (PA8) = RPWM,  TIM1 CH2 (PA9) = LPWM           */
/* USB          : PA11 = D-,  PA12 = D+                                      */
/* ADC          : PA0 Steering | PA1 Throttle | PA2 Brake | PA3 Clutch      */
/*                PA4 Shifter-X | PA5 Shifter-Y                              */
/* Encoder      : PB4 = TIM3 CH1 (A),  PB5 = TIM3 CH2 (B)                  */
/* Buttons      : PB0-PB7  (INPUT_PULLUP)                                    */
/* Reverse sw   : PB8      (INPUT_PULLUP)                                    */

/* ── Firmware version ────────────────────────────────────────────────────── */
#define FW_VERSION_MAJOR  3
#define FW_VERSION_MINOR  0

/* ── Input mode ──────────────────────────────────────────────────────────── */
#define INPUT_MODE_POT      0
#define INPUT_MODE_ENCODER  1

/* ── Defaults ────────────────────────────────────────────────────────────── */
#define DEFAULT_WHEEL_RANGE       900
#define DEFAULT_PULLEY_RATIO      4.5f
#define DEFAULT_POT_DEGREES       270.0f
#define DEFAULT_ENCODER_PPR       600
#define DEFAULT_FFB_STRENGTH      80
#define DEFAULT_SPRING_GAIN       65
#define DEFAULT_DAMPER_GAIN       40
#define DEFAULT_FRICTION_GAIN     25
#define DEFAULT_INERTIA_GAIN      20
#define DEFAULT_MAX_TORQUE        90
#define DEFAULT_CENTER_DEADZONE   0
#define DEFAULT_FILTER_STRENGTH   2
#define DEFAULT_BUMPSTOP_STRENGTH 60
#define DEFAULT_CENTER_SPRING     0
#define DEFAULT_TORQUE_SMOOTH     30

/* ── ADC ─────────────────────────────────────────────────────────────────── */
#define ADC_CHANNELS  6
#define ADC_MAX       4095
#define ADC_IDX_STEER   0
#define ADC_IDX_THR     1
#define ADC_IDX_BRAKE   2
#define ADC_IDX_CLUTCH  3
#define ADC_IDX_SHFT_X  4
#define ADC_IDX_SHFT_Y  5

/* ── PWM ─────────────────────────────────────────────────────────────────── */
#define PWM_FREQ_HZ  20000
#define PWM_MAX      999

/* ── Flash ───────────────────────────────────────────────────────────────── */
#define CONFIG_FLASH_ADDR  0x08060000UL
#define CONFIG_MAGIC       0xFFBB0300UL   /* V3.0 — changes with struct layout */

/* ── HID axis range ──────────────────────────────────────────────────────── */
#define HID_AXIS_MIN  -32767
#define HID_AXIS_MAX   32767

/* ── Pedal curve ─────────────────────────────────────────────────────────── */
#define CURVE_POINTS  5   /* 5-point custom curve: output% at 0,25,50,75,100% input */

/* ── Gear ────────────────────────────────────────────────────────────────── */
typedef enum { GEAR_N=0, GEAR_1, GEAR_2, GEAR_3,
               GEAR_4, GEAR_5, GEAR_6, GEAR_R } Gear_t;

/* ── Config struct v2.5 ─────────────────────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint32_t magic;

    /* Wheel geometry */
    uint16_t wheel_range;
    float    pulley_ratio;
    float    pot_degrees;
    uint8_t  steer_invert;
    uint16_t steer_center;
    uint16_t steer_min;
    uint16_t steer_max;

    /* Input source */
    uint8_t  input_mode;        /* INPUT_MODE_POT or INPUT_MODE_ENCODER      */
    uint16_t encoder_ppr;       /* pulses per revolution                     */

    /* FFB gains */
    uint8_t  ffb_strength;
    uint8_t  spring_gain;
    uint8_t  damper_gain;
    uint8_t  friction_gain;
    uint8_t  inertia_gain;
    uint8_t  max_torque;

    /* Advanced FFB */
    uint8_t  filter_strength;   /* 0=off 1=light 2=medium 3=heavy            */
    uint8_t  bumpstop_strength; /* 0-100 active return force at lock         */
    uint8_t  center_spring;     /* 0-100 always-on center spring             */
    uint8_t  torque_smooth;     /* 0-100 output smoothing                    */

    /* Misc */
    uint8_t  center_deadzone;

    /* Pedals */
    uint16_t thr_min,    thr_max;
    uint16_t brake_min,  brake_max;
    uint16_t clutch_min, clutch_max;
    uint8_t  thr_curve;         /* 0=custom 1=linear 2=square 3=cubic        */
    uint8_t  brake_curve;
    uint8_t  clutch_curve;
    uint8_t  thr_pts[CURVE_POINTS];    /* custom curve output% at 0,25,50,75,100% */
    uint8_t  brake_pts[CURVE_POINTS];
    uint8_t  clutch_pts[CURVE_POINTS];

    /* Shifter */
    uint16_t shft_x_left;
    uint16_t shft_x_right;
    uint16_t shft_y_fwd;
    uint16_t shft_y_rev;

    uint8_t  padding[4];
} WheelConfig_t;

extern WheelConfig_t g_cfg;
extern volatile uint16_t g_adc[ADC_CHANNELS];

#endif /* FFB_WHEEL_H */
