#ifndef ADC_INPUT_H
#define ADC_INPUT_H

#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_dma.h"
#include "ffb_wheel.h"

/* Init ADC1 + DMA on PA0-PA5 (6 channels, continuous circular scan)        */
void ADC_Input_Init(void);

/* Update EMA filter — call every 5ms from main loop (HID tick)             */
void ADC_Filter_Update(void);

/* Pedal axis: calibrated + curve applied, returns 0 to +32767              */
int16_t ADC_GetAxis(uint8_t axis);

/* Steering angle in degrees (pot mode only)                                */
float ADC_GetWheelAngle(void);

/* Gear from H-pattern shifter pots                                         */
Gear_t ADC_GetGear(void);

/* Raw unfiltered ADC value 0-4095 (diagnostics only)                       */
uint16_t ADC_GetRaw(uint8_t ch);

/* Filtered ADC value 0-4095                                                */
uint16_t ADC_GetRawFiltered(uint8_t ch);

/* DMA handle — used by DMA2_Stream0_IRQHandler in stm32f4xx_it.c           */
DMA_HandleTypeDef* ADC_GetDMAHandle(void);

#endif /* ADC_INPUT_H */
