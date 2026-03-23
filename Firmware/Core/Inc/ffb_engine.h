#ifndef FFB_ENGINE_H
#define FFB_ENGINE_H

#include <stdint.h>

/* Maximum simultaneous FFB effects DirectInput can load */
#define FFB_MAX_EFFECTS  8

/* Effect type IDs matching PID HID usage values */
#define FFB_ET_NONE           0x00
#define FFB_ET_CONSTANT       0x01
#define FFB_ET_SPRING         0x02
#define FFB_ET_DAMPER         0x03
#define FFB_ET_FRICTION       0x04
#define FFB_ET_INERTIA        0x05
#define FFB_ET_SQUARE         0x06
#define FFB_ET_SINE           0x07
#define FFB_ET_TRIANGLE       0x08

/* Effect state flags */
#define FFB_STATE_FREE        0x00
#define FFB_STATE_ALLOCATED   0x01
#define FFB_STATE_PLAYING     0x02

typedef struct {
    uint8_t  state;
    uint8_t  type;
    int16_t  magnitude;       /* Constant / periodic magnitude               */
    uint16_t period_ms;       /* Periodic: period                            */
    int16_t  offset;          /* Spring/Damper: offset                       */
    uint16_t deadband;        /* Spring: dead band                           */
    int16_t  positive_coeff;  /* Spring/Damper coefficient (positive dir)    */
    int16_t  negative_coeff;  /* Spring/Damper coefficient (negative dir)    */
    int16_t  attack_level;
    int16_t  fade_level;
    uint16_t attack_time;
    uint16_t fade_time;
    uint32_t duration_ms;
    uint32_t start_ms;        /* HAL_GetTick() when effect started           */
    uint16_t phase;           /* Periodic: phase offset                      */
} FFB_Effect_t;

/* Initialise FFB effect table */
void FFB_Init(void);

/* Called from USB HID OUT callback with raw PID report bytes */
void FFB_ProcessReport(uint8_t report_id, uint8_t *data, uint16_t len);

/* Calculate and return combined motor torque for current wheel state.
   wheel_angle_deg: current steering angle in degrees
   wheel_velocity:  degrees per second (approximate)
   Returns: -1000 to +1000                                                   */
int32_t FFB_Calculate(float wheel_angle_deg, float wheel_velocity);

/* Allocate a new effect slot, return index (0-based) or -1 if full */
int8_t  FFB_AllocEffect(void);

/* Free an effect slot */
void    FFB_FreeEffect(uint8_t idx);

/* Global device gain (0-255, set by host) */
extern uint8_t g_ffb_device_gain;

/* Global FFB enable flag */
extern uint8_t g_ffb_enabled;

#endif
