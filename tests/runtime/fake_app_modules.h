#ifndef FAKE_APP_MODULES_H
#define FAKE_APP_MODULES_H
#include "app.h"
#include "power.h"
#include "storage.h"
extern Sensors_Snapshot app_sensors;
extern Charging_Observation app_charge;
extern Vibration_Observation app_vibration;
extern Heater_Snapshot app_heater;
extern Power_State_t app_power;
extern Storage_Settings app_settings;
extern unsigned app_loads, app_validations, app_validation_ends, app_prepare_sleep;
extern unsigned app_shipping, app_sensor_updates, app_charge_updates, app_vibration_updates;
extern unsigned app_status_acks;
extern bool app_sensing_power, app_vibration_power, app_vibration_enabled, app_heat_authorized;
extern uint8_t app_vibration_level, app_heat_level;
extern uint32_t app_heat_current;
extern App_Bindings app_received_bindings;
extern bool app_charge_demand, app_charge_eligible, app_charge_normal, app_charge_fault;
extern bool app_charge_update_eligible, app_charge_update_normal, app_charge_update_fault;
extern uint32_t app_charge_battery_sequence;
extern bool app_sensor_quiescent, app_sensor_stop_ready;
extern I2C_DeviceResult app_drv_result;
void FakeApp_Reset(void);
#endif
