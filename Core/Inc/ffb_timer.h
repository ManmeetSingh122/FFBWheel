#ifndef FFB_TIMER_H
#define FFB_TIMER_H

/*
 * ffb_timer.h / ffb_timer.c
 *
 * Replaces the HAL_GetTick() polling loop in main.c with a hardware
 * TIM2 interrupt at exactly 1 kHz.
 *
 * Why this matters:
 *   HAL_GetTick() in a while(1) loop can skip ticks or fire twice if
 *   a USB interrupt takes longer than 1ms. A hardware timer interrupt
 *   fires at exactly 1 kHz regardless of what else is happening.
 *
 * Usage in main.c:
 *   1. Call FFB_Timer_Init() once after all other inits.
 *   2. Remove the t_ffb timing block from the while(1) loop entirely.
 *   3. The FFB loop now runs automatically from the interrupt.
 *   4. Keep the HID report and serial live in the while(1) loop as-is.
 */

#include <stdint.h>

/* Call once in main() after Motor_Init(), ADC_Input_Init(), FFB_Init() */
void FFB_Timer_Init(void);

/* Called automatically from TIM2 IRQ — do NOT call manually */
void FFB_Timer_Tick(void);

#endif /* FFB_TIMER_H */
