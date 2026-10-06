#include "runtime.h"
#include "firmware_hw.h"
#include "power.h"
#include <stddef.h>
static bool initialized;
static uint32_t last_tick;
static uint8_t pending_wake;
static Runtime_Observation observation;
void Runtime_Init(ADC_HandleTypeDef *adc, I2C_HandleTypeDef *charger,
                  I2C_HandleTypeDef *motor, TIM_HandleTypeDef *heater,
                  const Runtime_Profiles *profiles)
{
    if (initialized) return;
    App_Bindings bindings = {0};
    FirmwareHW_Init(&bindings, adc, charger, motor, heater);
    if (profiles != NULL) {
        bindings.charging = profiles->charging;
        bindings.vibration = profiles->vibration;
    }
    last_tick = HAL_GetTick();
    Power_InitWakePolling();
    App_Init(&bindings, last_tick);
    initialized = true;
}
void Runtime_Poll(void)
{
    if (!initialized) return;
    uint32_t now = HAL_GetTick();
    uint8_t wake = Power_PollWake();
    if ((wake & POWER_WAKE_CHARGER) != 0U) Charging_OnInterrupt();
    if (App_GetSnapshot().sleep_requested) pending_wake |= wake;
    if (observation.polling_sleep) {
        /* Track time while asleep; sleeping slots are intentionally absent. */
        last_tick = now;
        wake |= pending_wake;
        if (wake == 0U) return;
        pending_wake = 0U;
        observation.polling_sleep = false;
        App_Wake(now, (wake & POWER_WAKE_BUTTON) != 0U ? APP_WAKE_POWER : APP_WAKE_CHARGER,
                 false, now);
        return;
    }
    if (now == last_tick) return;
    uint32_t missed = (uint32_t)(now - last_tick) - 1U;
    if (UINT32_MAX - observation.missed_ticks < missed) observation.missed_ticks = UINT32_MAX;
    else observation.missed_ticks += missed;
    last_tick = now;
    App_Update(now, true);
    App_Snapshot app = App_GetSnapshot();
    if (app.sleep_requested) pending_wake |= wake;
    if (app.sleep_ready) observation.polling_sleep = true;
}
Runtime_Observation Runtime_GetObservation(void) { return observation; }
