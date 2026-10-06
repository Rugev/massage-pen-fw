/* Real module integration with synthetic devices, never a board profile. */
#include "app.h"
#include "fake_hal.h"
#include "fake_i2c.h"
#include "mp2724.h"
#include "main.h"
#include <assert.h>
#include <stdio.h>
static void missing_bindings_remain_gated(void)
{
    FakeHAL_Reset();
    FakeHAL_SetPowerGood(GPIO_PIN_SET);
    FakeHAL_SetInput(BUTTON_PWR_ON_GPIO_Port, BUTTON_PWR_ON_Pin, GPIO_PIN_SET);
    App_Init(NULL, 0U);
    for (uint32_t now = 0U; now <= 1200U; ++now) {
        FakeHAL_SetTick(now);
        App_Update(now, true);
    }
    assert(App_GetSnapshot().state == APP_STARTUP);
    assert(App_GetSnapshot().fault_code == 0U);
    assert(!Sensors_GetSnapshot().available && !Sensors_GetSnapshot().ready);
    assert(Sensors_IsQuiescent());
    assert(!Charging_GetObservation().profile_valid);
    assert(!Charging_GetObservation().configuration_ready);
    assert(!Charging_GetObservation().ntc_fresh);
    assert(!Vibration_GetObservation().profile_valid);
    assert(!Vibration_GetObservation().output_active);
    assert(!Heater_GetSnapshot().available && Heater_GetSnapshot().duty_ppm == 0U);
    assert(!App_GetSnapshot().sleep_ready);
    puts("app real-module missing-adapter/profile gating passed");
}

static bool idle_leased;
static bool acquire_idle(uint8_t reg)
{
    assert(reg == MP2724_REG_IIN || reg == MP2724_REG_CHG_CTRL3);
    assert(!idle_leased);
    idle_leased = true;
    return true; /* Synthetic device is quiescent for the complete transaction. */
}
static void release_idle(uint8_t reg)
{
    assert(reg == MP2724_REG_IIN || reg == MP2724_REG_CHG_CTRL3);
    assert(idle_leased);
    idle_leased = false;
}
static void integrated_tick(uint32_t now)
{
    FakeHAL_SetTick(now);
    uint32_t triggers = FakeHAL_TriggerCount();
    App_Update(now, true);
    if (FakeHAL_TriggerCount() != triggers && Sensors_GetSnapshot().available)
        FakeHAL_CompletePair(3276U * SENSORS_ADC_OVERSAMPLING_RATIO,
                             1912U * SENSORS_ADC_OVERSAMPLING_RATIO, SENSORS_CHANNEL_ALL);
    if (FakeI2C_HasPending(&fake_charger_i2c)) FakeI2C_Complete(&fake_charger_i2c);
    if (FakeI2C_HasPending(&fake_driver_i2c)) FakeI2C_Complete(&fake_driver_i2c);
}
static void battery_only_validation_finishes(bool enable)
{
    /* Reuse the existing synthetic module-test profiles. Their encodings are
     * explicit test agreements and do not resolve battery-only NTC freshness. */
    const Charging_IdleOps idle = {acquire_idle, release_idle};
    const Charging_Profile charger = {.agreed=true,
        .registers={0x10,0,0x63,0x13,0x06,0x18,0x04,0x1e,0x20,0x03,0x20,0x51,0x21,0x4e,0,0},
        .idle=&idle};
    const Vibration_Profile motor = {.validated=true,
        .mode=0x08, .control=0x80, .feedback_control=0x52, .rated_voltage=0x33, .od_clamp=0x44,
        .lra_drive_control=0x07, .bemf_timing=0x22, .timing_control=0x0c, .auto_cal_time=0,
        .calibration_duration_ms=250, .calibration_timeout_ms=400, .rtp={0,31,63,127}};
    FakeI2C_Reset(); FakeHAL_ResetSensors(); idle_leased=false;
    FakeHAL_SetPowerGood(GPIO_PIN_SET);
    FakeHAL_SetInput(BUTTON_PWR_ON_GPIO_Port, BUTTON_PWR_ON_Pin, enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
    const App_Bindings bindings = {.adc=&fake_adc, .sensors=FakeHAL_SensorOps(),
        .charger_i2c=&fake_charger_i2c, .vibration_i2c=&fake_driver_i2c,
        .charger_transport=FakeI2C_Ops(), .vibration_transport=FakeI2C_Ops(),
        .charging=&charger, .vibration=&motor};
    App_Init(&bindings, 0U);
    for (uint32_t now=0U; now<=1200U; ++now) {
        integrated_tick(now);
        if (enable && App_GetSnapshot().state == APP_NORMAL) break;
    }
    assert(App_GetSnapshot().fault_code == 0U);
    assert(!Charging_GetObservation().input_valid && !Charging_GetObservation().input_ready);
    assert(!Charging_GetObservation().ntc_fresh);
    if (enable) {
        assert(App_GetSnapshot().state == APP_NORMAL);
        assert(Charging_GetObservation().configuration_ready);
        assert(Sensors_GetSnapshot().ready);
    } else {
        assert(App_GetSnapshot().state == APP_SLEEP);
        assert(App_GetSnapshot().sleep_ready && !App_GetSnapshot().shutdown_pending);
        assert(Sensors_IsQuiescent());
    }
    assert(!idle_leased);
}
int main(void)
{
    missing_bindings_remain_gated();
    battery_only_validation_finishes(false);
    battery_only_validation_finishes(true);
    puts("app real-module battery-only validation reaches Sleep and Normal without NTC freshness");
}
