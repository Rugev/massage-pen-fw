#ifndef USER_RUNTIME_H
#define USER_RUNTIME_H
#include "app.h"
typedef struct {
    const Charging_Profile *charging;
    const Vibration_Profile *vibration;
} Runtime_Profiles;
typedef struct { bool polling_sleep; uint32_t missed_ticks; } Runtime_Observation;
/* Boot once, after generated peripheral init. NULL profiles keep policy gates. */
void Runtime_Init(ADC_HandleTypeDef *adc, I2C_HandleTypeDef *charger,
                  I2C_HandleTypeDef *motor, TIM_HandleTypeDef *heater,
                  const Runtime_Profiles *profiles);
/* Tight foreground polling; dispatch at most once per distinct HAL tick. */
void Runtime_Poll(void);
Runtime_Observation Runtime_GetObservation(void);
#endif
