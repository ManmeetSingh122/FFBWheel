#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>

/* Quadrature encoder on TIM3 CH1/CH2 (PB4/PB5).
   TIM3 runs in encoder mode — hardware counts A/B edges automatically.
   No interrupts needed; just read the counter.                              */

/* Call once after Config_Load() when input_mode == INPUT_MODE_ENCODER      */
void Encoder_Init(uint16_t ppr);

/* Set the logical zero position (call when wheel is centered)              */
void Encoder_SetCenter(void);

/* Return current wheel angle in degrees based on encoder count.
   Range is ±(wheel_range/2) — same as ADC_GetWheelAngle().                */
float Encoder_GetAngle(void);

/* Return raw encoder count (signed, relative to center)                    */
int32_t Encoder_GetCount(void);

#endif /* ENCODER_H */
