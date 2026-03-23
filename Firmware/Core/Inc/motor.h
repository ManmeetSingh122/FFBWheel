#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>

/* Initialise TIM1 CH1/CH2 for 20 kHz PWM on PA8 (RPWM) and PA9 (LPWM).
   Call once after HAL_Init() and SystemClock_Config().                      */
void Motor_Init(void);

/* Set motor torque.
   value : -1000 to +1000
     positive → forward (RPWM active, LPWM=0)
     negative → reverse (LPWM active, RPWM=0)
     zero     → coast (both 0)
   max_pct: 0-100 safety ceiling from config                                 */
void Motor_Set(int32_t value, uint8_t max_pct);

/* Hard stop – both channels 0 */
void Motor_Stop(void);

#endif
