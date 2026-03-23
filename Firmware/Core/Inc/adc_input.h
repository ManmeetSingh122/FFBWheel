#ifndef ADC_INPUT_H
#define ADC_INPUT_H

#include "ffb_wheel.h"

/* Initialise ADC1 with DMA on PA0-PA5 (6 channels, continuous scan).
   Results are always fresh in g_adc[].                                       */
void ADC_Input_Init(void);

/* Apply calibration and curve, return HID-range value (-32767 to +32767).
   axis: ADC_IDX_* constant                                                   */
int16_t ADC_GetAxis(uint8_t axis);

/* Return current wheel angle in degrees (-range/2 to +range/2) */
float   ADC_GetWheelAngle(void);

/* Detect current gear from shifter pots */
Gear_t  ADC_GetGear(void);

/* Read raw ADC value for a channel (0-4095) */
uint16_t ADC_GetRaw(uint8_t ch);

#endif
