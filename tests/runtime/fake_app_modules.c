#include "fake_app_modules.h"
#include <string.h>
Sensors_Snapshot app_sensors;
Charging_Observation app_charge;
Vibration_Observation app_vibration;
Heater_Snapshot app_heater;
Power_State_t app_power;
Storage_Settings app_settings;
unsigned app_loads, app_validations, app_validation_ends, app_prepare_sleep;
unsigned app_shipping, app_sensor_updates, app_charge_updates, app_vibration_updates;
unsigned app_status_acks;
bool app_sensing_power, app_vibration_power, app_vibration_enabled, app_heat_authorized;
uint8_t app_vibration_level, app_heat_level;
uint32_t app_heat_current;
App_Bindings app_received_bindings;
bool app_sensor_quiescent, app_sensor_stop_ready;
I2C_DeviceResult app_drv_result;
void FakeApp_Reset(void) {
    memset(&app_sensors, 0, sizeof app_sensors);
    memset(&app_charge, 0, sizeof app_charge);
    memset(&app_vibration, 0, sizeof app_vibration);
    memset(&app_heater, 0, sizeof app_heater);
    app_settings = (Storage_Settings){1U, 1U};
    app_loads = app_validations = app_validation_ends = app_prepare_sleep = 0U;
    app_shipping = app_sensor_updates = app_charge_updates = app_vibration_updates = 0U;
    app_status_acks = 0U;
    app_heat_authorized = app_sensing_power = app_vibration_power = app_vibration_enabled = false;
    app_vibration_level = app_heat_level = 0U;
    app_heat_current = 0U;
    app_received_bindings = (App_Bindings){0};
    app_sensor_quiescent = app_sensor_stop_ready = true;
    app_drv_result = (I2C_DeviceResult){.state=I2C_RESULT_IDLE};
}
void Power_Init(void) { app_power = POWER_STATE_DISABLED; }
void Power_Enable(void) { if (app_power == POWER_STATE_DISABLED) app_power = POWER_STATE_STARTING; }
void Power_Disable(void) { app_power = POWER_STATE_DISABLED; }
void Power_Update(void) {}
Power_State_t Power_GetState(void) { return app_power; }
void Sensors_Init(ADC_HandleTypeDef *adc, const Sensors_AcquisitionOps *ops) {app_received_bindings.adc=adc; app_received_bindings.sensors=ops;}
void Sensors_Update(bool power) {
    app_sensing_power = power; app_sensor_updates++;
    if (power) app_sensor_quiescent=false;
    else if (app_sensor_stop_ready) app_sensor_quiescent=true;
}
bool Sensors_IsQuiescent(void) {return app_sensor_quiescent;}
Sensors_Snapshot Sensors_GetSnapshot(void) {return app_sensors;}
void MP2724_Init(I2C_HandleTypeDef *handle, const I2C_DeviceOps *ops) {app_received_bindings.charger_i2c=handle; app_received_bindings.charger_transport=ops;}
void DRV2624_Init(I2C_HandleTypeDef *handle, const I2C_DeviceOps *ops) {app_received_bindings.vibration_i2c=handle; app_received_bindings.vibration_transport=ops;}
I2C_DeviceResult DRV2624_GetResult(void) {return app_drv_result;}
bool DRV2624_AcknowledgeStatus(uint32_t sequence) {
    /* The fake exposes a retained shared observation, never invents a read. */
    if (!app_vibration.status.valid || sequence != app_vibration.status.sequence) return false;
    app_status_acks++;
    app_vibration.status.valid = false;
    return true;
}
void Charging_Init(const Charging_Profile *profile) {app_received_bindings.charging=profile;}
void Charging_BeginValidation(void) {app_validations++;}
void Charging_EndValidation(void) {app_validation_ends++;}
void Charging_SetChargeRequired(bool required) {(void)required;}
void Charging_Update(bool available) {(void)available; app_charge_updates++;}
void Charging_RequestPrepareSleep(void) {app_prepare_sleep++;}
bool Charging_RequestShipping(void) {app_shipping++; return true;}
Charging_Observation Charging_GetObservation(void) {return app_charge;}
void Vibration_Init(const Vibration_Profile *profile) {app_received_bindings.vibration=profile;}
void Vibration_Update(bool power, bool enabled, uint8_t level) {
    app_vibration_updates++;
    app_vibration_power = power; app_vibration_enabled = enabled; app_vibration_level = level;
}
Vibration_Observation Vibration_GetObservation(void) {return app_vibration;}
void Heater_Init(const Heater_PwmOps *ops, void *context) {app_received_bindings.heater_pwm=ops; app_received_bindings.heater_context=context;}
void Heater_Update(uint32_t now, uint8_t level, bool authorized, uint32_t current,
                   const Sensors_Snapshot *sensors) {
    (void)now; (void)sensors;
    app_heat_level = level; app_heat_authorized = authorized; app_heat_current = current;
}
Heater_Snapshot Heater_GetSnapshot(void) {return app_heater;}
Storage_Settings Storage_Load(void) {app_loads++; return app_settings;}
