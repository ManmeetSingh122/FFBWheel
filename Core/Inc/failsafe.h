#ifndef FAILSAFE_H
#define FAILSAFE_H

#include <stdint.h>

/*
 * failsafe.h / failsafe.c
 *
 * Three independent failsafe mechanisms:
 *
 * 1. IWDG Watchdog — if firmware freezes/crashes, watchdog resets the MCU
 *    after ~500ms. Motor goes to 0 on reset because TIM1 restarts at 0.
 *
 * 2. USB Timeout — if USB cable is unplugged or game closes while motor
 *    is running, motor is stopped after 200ms of no USB HID traffic.
 *
 * 3. Stuck motor detection — if motor is commanded at >80% PWM for more
 *    than 2 seconds continuously with no wheel movement (pot stuck or
 *    belt snapped), motor is cut and failsafe flag is set.
 */

/* Call once in main() before the main loop */
void Failsafe_Init(void);

/* Call every time a USB HID OUT report is received (FFB traffic) */
void Failsafe_KickUSB(void);

/* Call every time a USB HID IN report is sent (gamepad report) */
void Failsafe_KickHID(void);

/* Pet the watchdog — call from main loop (or TIM2 tick) */
void Failsafe_KickWDT(void);

/* Called from TIM2 tick (1 kHz) to update timeout counters
   Pass current motor torque and wheel angle for stuck detection          */
void Failsafe_Update(int32_t torque, float angle);

/* Returns 1 if any failsafe is triggered — caller should Motor_Stop()   */
uint8_t Failsafe_IsTriggered(void);

/* Clear triggered state (called after USB reconnects)                   */
void Failsafe_Clear(void);

/* Human-readable reason for last trigger (for serial debug output)      */
const char* Failsafe_Reason(void);

#endif /* FAILSAFE_H */
