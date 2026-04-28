#ifndef USB_HID_DESC_H
#define USB_HID_DESC_H

#include <stdint.h>

/* ── Report IDs ──────────────────────────────────────────────────────────── */
#define REPORT_ID_INPUT          0x01  /* Gamepad axes + buttons (IN)        */
#define REPORT_ID_SET_EFFECT     0x02  /* PID Set Effect (OUT)               */
#define REPORT_ID_SET_ENVELOPE   0x03  /* PID Set Envelope (OUT)             */
#define REPORT_ID_SET_CONDITION  0x04  /* PID Set Condition (OUT)            */
#define REPORT_ID_SET_PERIODIC   0x05  /* PID Set Periodic (OUT)             */
#define REPORT_ID_SET_CONSTANT   0x06  /* PID Set Constant Force (OUT)       */
#define REPORT_ID_OPERATION      0x07  /* PID Effect Operation (OUT)         */
#define REPORT_ID_DEVICE_CTRL    0x08  /* PID Device Control (OUT)           */
#define REPORT_ID_DEVICE_GAIN    0x09  /* PID Device Gain (OUT)              */
#define REPORT_ID_CREATE_EFFECT  0x0A  /* PID Create New Effect (FEATURE)    */
#define REPORT_ID_BLOCK_LOAD     0x0B  /* PID Block Load Report (IN)         */
#define REPORT_ID_PID_STATE      0x0C  /* PID State Report (IN)              */

/* ── Input Report (sent to PC) ───────────────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint8_t  report_id;     /* always REPORT_ID_INPUT                        */
    int16_t  steering;      /* -32767 to +32767                              */
    int16_t  throttle;      /* -32767 to +32767                              */
    int16_t  brake;         /* -32767 to +32767                              */
    int16_t  clutch;        /* -32767 to +32767                              */
    uint8_t  buttons;       /* bit 0-7 = button 1-8                         */
    uint8_t  gear;          /* 0=N 1-6=gears 7=R                            */
} HID_InputReport_t;

/* ── PID Out Reports (received from PC / game) ───────────────────────────── */
typedef struct __attribute__((packed)) {
    uint8_t  report_id;
    uint8_t  effect_block_index;   /* 1-based slot number                   */
    uint8_t  effect_type;          /* ET_* constants                        */
    uint16_t duration;             /* ms, 0xFFFF = infinite                 */
    uint16_t trigger_repeat_int;
    uint16_t sample_period;
    uint8_t  gain;                 /* 0-255                                 */
    uint8_t  trigger_button;
    uint8_t  axes_enable;
    uint8_t  direction_enable;
    int16_t  direction;
} PID_SetEffect_t;

typedef struct __attribute__((packed)) {
    uint8_t  report_id;
    uint8_t  effect_block_index;
    int16_t  attack_level;
    uint16_t attack_time;
    int16_t  fade_level;
    uint16_t fade_time;
} PID_SetEnvelope_t;

typedef struct __attribute__((packed)) {
    uint8_t  report_id;
    uint8_t  effect_block_index;
    uint8_t  param_block_offset;
    int16_t  cp_offset;
    int16_t  positive_coeff;
    int16_t  negative_coeff;
    uint16_t positive_sat;
    uint16_t negative_sat;
    uint16_t dead_band;
} PID_SetCondition_t;

typedef struct __attribute__((packed)) {
    uint8_t  report_id;
    uint8_t  effect_block_index;
    int16_t  magnitude;
    int16_t  offset;
    uint16_t phase;
    uint16_t period;
} PID_SetPeriodic_t;

typedef struct __attribute__((packed)) {
    uint8_t  report_id;
    uint8_t  effect_block_index;
    int16_t  magnitude;
} PID_SetConstant_t;

typedef struct __attribute__((packed)) {
    uint8_t  report_id;
    uint8_t  effect_block_index;
    uint8_t  operation;    /* 1=Start 2=StartSolo 3=Stop                    */
    uint8_t  loop_count;
} PID_EffectOperation_t;

typedef struct __attribute__((packed)) {
    uint8_t  report_id;
    uint8_t  control;      /* 1=Enable 2=Disable 4=Stop_all 8=Reset 16=Pause*/
} PID_DeviceControl_t;

typedef struct __attribute__((packed)) {
    uint8_t  report_id;
    uint8_t  device_gain;  /* 0-255                                         */
} PID_DeviceGain_t;

/* Block load response (sent back to PC after CreateNewEffect) */
typedef struct __attribute__((packed)) {
    uint8_t  report_id;    /* REPORT_ID_BLOCK_LOAD                          */
    uint8_t  effect_block_index;
    uint8_t  load_status;  /* 1=Success 2=Full 3=Error                     */
    uint16_t ram_pool_avail;
} PID_BlockLoad_t;

/* PID State (sent periodically to PC) */
typedef struct __attribute__((packed)) {
    uint8_t  report_id;    /* REPORT_ID_PID_STATE                           */
    uint8_t  device_paused      : 1;
    uint8_t  actuators_enabled  : 1;
    uint8_t  safety_switch      : 1;
    uint8_t  actuator_override  : 1;
    uint8_t  actuator_power     : 1;
    uint8_t  padding            : 3;
    uint8_t  effect_playing;
    uint8_t  effect_block_index;
} PID_State_t;

/* ── HID Report Descriptor bytes (defined in usb_hid_desc.c) ─────────────── */
extern const uint8_t HID_ReportDescriptor[];
extern const uint16_t HID_ReportDescriptorSize;

#define HID_REPORT_DESC_SIZE  743

#endif /* USB_HID_DESC_H */
