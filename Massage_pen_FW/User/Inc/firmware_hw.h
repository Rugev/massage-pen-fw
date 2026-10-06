#ifndef USER_FIRMWARE_HW_H
#define USER_FIRMWARE_HW_H
#include "app.h"
#define FIRMWARE_HW_ADC_TIP_INDEX 0U
#define FIRMWARE_HW_ADC_BATTERY_INDEX 1U
#define FIRMWARE_HW_I2C_ABORT_TIMEOUT_MS I2C_DEVICE_TIMEOUT_MS
/* Dedicated generated ADC1/DMA1 CH1, I2C1 charger, I2C2 motor, TIM1 CH1.
 * Validates bindings; unavailable/mismatched mechanisms remain NULL. */
void FirmwareHW_Init(App_Bindings *bindings, ADC_HandleTypeDef *adc,
                     I2C_HandleTypeDef *charger, I2C_HandleTypeDef *motor,
                     TIM_HandleTypeDef *heater);
#endif
