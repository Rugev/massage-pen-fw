#include "fake_app_modules.h"
#include "fake_hal.h"
#include "buttons.h"
#include "leds.h"
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define CHECK(value) do { if (!(value)) { fprintf(stderr, "%s:%d: %s\n", __func__, __LINE__, #value); exit(1); } } while (0)
static void tick(uint32_t now) { FakeHAL_SetTick(now); App_Update(now, true); }
static App_State state(void) { return App_GetSnapshot().state; }
static void raw_power(bool down) { FakeHAL_SetInput(BUTTON_PWR_ON_GPIO_Port, BUTTON_PWR_ON_Pin, down); }
static void reset(uint32_t now) {
    FakeHAL_Reset(); FakeApp_Reset(); FakeHAL_SetTick(now);
    App_Init(NULL, now);
}
static void ready(void) {
    app_power = POWER_STATE_READY;
    app_sensors = (Sensors_Snapshot){.available=true, .ready=true, .fresh=true,
        .battery_fresh=true, .battery_current_valid=true, .battery_sequence=1U, .battery_valid=true, .tip_valid=true, .battery_mv=3600U,
        .filtered_battery_mv=3600U, .tip_mdegc=25000, .filtered_tip_mdegc=25000};
    app_charge.profile_valid = app_charge.configuration_ready = app_charge.status_ready = true;
    app_vibration.profile_valid = app_vibration.configuration_ready = true;
}
static void normal(uint32_t base) {
    reset(base); ready(); raw_power(true); tick(base); tick(base+20U); tick(base+320U);
    CHECK(state() == APP_NORMAL);
}
static void release_power(uint32_t now) { raw_power(false); tick(now); tick(now+20U); }
static void click(Buttons_Id id, uint32_t now, uint32_t duration) {
    GPIO_TypeDef *port = id == BUTTONS_HEAT ? BUTTON_HEAT_GPIO_Port : BUTTON_VIBRATION_GPIO_Port;
    uint16_t pin = id == BUTTONS_HEAT ? BUTTON_HEAT_Pin : BUTTON_VIBRATION_Pin;
    FakeHAL_SetInput(port, pin, GPIO_PIN_SET); tick(now); tick(now+20U);
    FakeHAL_SetInput(port, pin, GPIO_PIN_RESET); tick(now+duration); tick(now+duration+20U);
}
/* Real App + Buttons: shutdown stays cached while physical sleep hides release. */
static uint32_t shutdown_with_cached_hold(uint32_t base)
{
    normal(base);
    release_power(base + 400U);
    raw_power(true);
    tick(base + 500U);
    tick(base + 520U);
    tick(base + 1520U);
    CHECK(state() == APP_SLEEP);
    app_charge.sleep_ready = true;
    tick(base + 1521U);
    CHECK(App_GetSnapshot().sleep_ready);
    CHECK(Buttons_IsPressed(BUTTONS_POWER));
    return Buttons_PressIdentity(BUTTONS_POWER);
}
static void test_verified_wake_after_sleeping_release_repress(void)
{
    for (unsigned rollover = 0U; rollover < 2U; ++rollover) {
        uint32_t base = rollover ? UINT32_MAX - 2100U : 0U;
        uint32_t shutdown_identity = shutdown_with_cached_hold(base);
        raw_power(false); /* Physically asleep: no App/Buttons updates. */
        FakeHAL_SetTick(base + 1600U);
        raw_power(true);
        uint32_t wake = base + 2000U;
        App_Wake(wake, APP_WAKE_POWER, true, wake);
        ready();
        app_charge.configuration_ready = false;
        tick(wake);
        CHECK(state() == APP_WAKEUP);
        tick(wake + 299U);
        CHECK(state() == APP_WAKEUP);
        tick(wake + 300U); /* Enable qualifies before configuration is ready. */
        CHECK(state() == APP_WAKEUP);
        uint32_t identity = Buttons_PressIdentity(BUTTONS_POWER);
        Buttons_AdoptWakePress(BUTTONS_POWER, wake);
        CHECK(Buttons_PressIdentity(BUTTONS_POWER) == identity);
        app_charge.configuration_ready = true;
        tick(wake + 400U);
        CHECK(state() == APP_NORMAL);
        CHECK(identity != shutdown_identity);
        CHECK(App_GetSnapshot().session_started_ms == wake + 400U);
        CHECK(app_loads == 2U && app_validations == 2U);
        Buttons_AdoptWakePress(BUTTONS_POWER, wake);
        tick(wake + 1000U);
        CHECK(state() == APP_NORMAL); /* Same enabling episode cannot shut down. */
        release_power(wake + 1100U);
        raw_power(true);
        tick(wake + 1200U);
        tick(wake + 1220U);
        tick(wake + 2219U);
        CHECK(state() == APP_NORMAL);
        tick(wake + 2220U);
        CHECK(state() == APP_SLEEP); /* A fresh episode still disables normally. */
    }
}
static void test_raw_power_wake_rearms_cached_hold_and_debounces(void)
{
    for (unsigned rollover = 0U; rollover < 2U; ++rollover) {
        uint32_t base = rollover ? UINT32_MAX - 2100U : 0U;
        uint32_t shutdown_identity = shutdown_with_cached_hold(base);
        raw_power(false); /* Sleeping release/repress is absent from debounce history. */
        FakeHAL_SetTick(base + 1600U);
        raw_power(true);
        uint32_t wake = base + 2000U;
        App_Wake(wake, APP_WAKE_POWER, false, 0U);
        ready();
        app_charge.configuration_ready = false;
        tick(wake);
        CHECK(!Buttons_IsPressed(BUTTONS_POWER));
        CHECK(Buttons_PressDuration(BUTTONS_POWER, wake) == 0U);
        tick(wake + 19U);
        CHECK(!Buttons_IsPressed(BUTTONS_POWER));
        tick(wake + 20U);
        CHECK(Buttons_IsPressed(BUTTONS_POWER));
        uint32_t identity = Buttons_PressIdentity(BUTTONS_POWER);
        CHECK(identity != 0U && identity != shutdown_identity);
        CHECK(Buttons_PressDuration(BUTTONS_POWER, wake + 20U) == 0U);
        tick(wake + 319U);
        CHECK(state() == APP_WAKEUP);
        tick(wake + 320U);
        CHECK(state() == APP_WAKEUP); /* Qualifies, but configuration is pending. */
        app_charge.configuration_ready = true;
        tick(wake + 400U);
        CHECK(state() == APP_NORMAL);
        CHECK(Buttons_PressIdentity(BUTTONS_POWER) == identity);
        tick(wake + 1020U);
        CHECK(state() == APP_NORMAL);
    }
}
static void test_charger_wake_keeps_continuing_shutdown_hold_consumed(void)
{
    uint32_t identity = shutdown_with_cached_hold(0U);
    app_charge.input_valid = true;
    App_Wake(2000U, APP_WAKE_CHARGER, false, 0U);
    ready();
    tick(2000U);
    tick(2400U);
    tick(3020U);
    CHECK(state() != APP_NORMAL && state() != APP_CHARGING_NORMAL);
    CHECK(Buttons_IsPressed(BUTTONS_POWER));
    CHECK(Buttons_PressIdentity(BUTTONS_POWER) == identity);
    CHECK(!app_heat_authorized && !app_vibration_enabled);
    release_power(3100U);
    CHECK(state() == APP_CHARGING);
    raw_power(true);
    tick(3200U);
    tick(3220U);
    tick(3520U);
    CHECK(state() == APP_CHARGING_NORMAL);
}
static void test_startup_and_readiness(void) {
    reset(0U); CHECK(state() == APP_STARTUP); CHECK(app_power == POWER_STATE_STARTING);
    tick(1U); CHECK(!app_sensing_power); CHECK(state() == APP_STARTUP);
    ready(); app_sensors.ready=false; tick(2U); CHECK(app_sensing_power); CHECK(state()==APP_STARTUP);
    app_sensors.ready=true; app_charge.configuration_ready=false; tick(3U); CHECK(state()==APP_STARTUP);
    app_charge.configuration_ready=true; tick(20U);
    CHECK(state()==APP_SLEEP); CHECK(app_loads==1U); CHECK(app_prepare_sleep==1U);
    CHECK(!app_heat_authorized); CHECK(!app_vibration_enabled);
    CHECK(app_power==POWER_STATE_DISABLED);
    unsigned charge_updates=app_charge_updates, vib_updates=app_vibration_updates;
    tick(21U); CHECK(app_charge_updates>charge_updates); CHECK(app_vibration_updates>vib_updates);
}
static void test_waking_press_validation_and_rollover(void) {
    reset(0U); ready(); tick(20U); CHECK(state()==APP_SLEEP);
    uint32_t start=UINT32_MAX-200U;
    raw_power(true); App_Wake(start, APP_WAKE_POWER, true, start);
    app_power=POWER_STATE_READY; app_charge.configuration_ready=false;
    tick(start+300U); CHECK(state()==APP_WAKEUP);
    app_charge.configuration_ready=true; tick(start+1200U);
    CHECK(state()==APP_NORMAL); CHECK(app_loads==2U); CHECK(app_validations==2U);
    tick(start+1500U); CHECK(state()==APP_NORMAL); /* Same waking press cannot disable. */
    release_power(start+1600U); raw_power(true); tick(start+1700U); tick(start+1720U);
    tick(start+2719U); CHECK(state()==APP_NORMAL);
    tick(start+2720U); CHECK(state()==APP_SLEEP);
    app_charge.configuration_ready=true; tick(start+5000U); CHECK(state()==APP_SLEEP);
}
static void test_short_wake_and_release_qualified_pending(void) {
    reset(0U); ready(); tick(20U);
    raw_power(true); App_Wake(100U, APP_WAKE_POWER, true, 100U); app_power=POWER_STATE_READY;
    app_sensors.ready=false; tick(150U); release_power(200U);
    app_sensors.ready=true; tick(300U); CHECK(state()==APP_SLEEP); CHECK(app_loads==2U);
    raw_power(true); App_Wake(400U, APP_WAKE_POWER, true, 400U); app_power=POWER_STATE_READY;
    app_charge.configuration_ready=false; release_power(710U);
    CHECK(state()==APP_WAKEUP); app_charge.configuration_ready=true; tick(800U);
    CHECK(state()==APP_NORMAL); /* A qualified release is retained until ready. */
}
static void test_cold_only_inference_and_low_precedence(void) {
    reset(0U); ready(); app_charge.input_valid=true;
    app_sensors.tip_mdegc=HEATER_FAULT_TEMP_MDEGC-APP_COLD_INFERENCE_MARGIN_MDEGC;
    tick(20U); CHECK(state()==APP_CHARGING);
    CHECK(App_GetSnapshot().heat_level==0U); CHECK(App_GetSnapshot().vibration_level==0U);
    app_charge.input_valid=false; tick(21U); CHECK(state()==APP_SLEEP);
    App_Wake(21U, APP_WAKE_CHARGER, false, 0U); app_power=POWER_STATE_READY; tick(41U);
    CHECK(App_GetSnapshot().heat_level==1U); CHECK(App_GetSnapshot().vibration_level==1U);
    reset(0U); ready(); app_charge.input_valid=true; app_charge.active_charging=true;
    app_sensors.battery_mv=BATTERY_STANDBY_MV;
    app_sensors.tip_mdegc=HEATER_FAULT_TEMP_MDEGC-APP_COLD_INFERENCE_MARGIN_MDEGC;
    tick(20U); CHECK(state()==APP_CHARGING); CHECK(App_GetSnapshot().heat_level==1U);
}
static void test_usb_session_and_gestures(void) {
    normal(0U); uint32_t started=App_GetSnapshot().session_started_ms;
    app_charge.input_valid=true; tick(400U); CHECK(state()==APP_CHARGING_NORMAL);
    app_charge.input_valid=false; tick(500U); CHECK(state()==APP_NORMAL);
    CHECK(App_GetSnapshot().session_started_ms==started);
    click(BUTTONS_HEAT, 600U, 100U); CHECK(App_GetSnapshot().heat_level==1U);
    click(BUTTONS_HEAT, 900U, 101U); CHECK(App_GetSnapshot().heat_level==2U);
    click(BUTTONS_HEAT, 1200U, 499U); CHECK(App_GetSnapshot().heat_level==3U);
    click(BUTTONS_HEAT, 1800U, 500U); CHECK(App_GetSnapshot().heat_level==3U);
    click(BUTTONS_HEAT, 2400U, 101U); CHECK(App_GetSnapshot().heat_level==0U);
    click(BUTTONS_VIBRATION, 2700U, 101U); CHECK(App_GetSnapshot().vibration_level==2U);
    tick(started+APP_SESSION_LIMIT_MS-1U); CHECK(state()==APP_NORMAL);
    tick(started+APP_SESSION_LIMIT_MS); CHECK(state()==APP_SLEEP);
}
static void test_zero_timer_rollover_and_disable(void) {
    uint32_t base=UINT32_MAX-1000U;
    normal(base); release_power(base+400U);
    click(BUTTONS_HEAT,base+500U,101U); click(BUTTONS_HEAT,base+700U,101U);
    click(BUTTONS_HEAT,base+900U,101U);
    click(BUTTONS_VIBRATION,base+1100U,101U); click(BUTTONS_VIBRATION,base+1300U,101U);
    click(BUTTONS_VIBRATION,base+1500U,101U);
    uint32_t zero=base+1621U;
    tick(zero+APP_ZERO_LEVEL_LIMIT_MS); CHECK(state()==APP_NORMAL);
    tick(zero+APP_ZERO_LEVEL_LIMIT_MS+1U); CHECK(state()==APP_SLEEP);
    normal(0U); release_power(400U); app_charge.input_valid=true;
    raw_power(true); tick(500U); tick(520U); tick(1520U);
    CHECK(state()==APP_CHARGING); CHECK(!app_heat_authorized); CHECK(!app_vibration_enabled);
}
static void test_heat_calibration_and_live_current(void) {
    normal(0U); CHECK(app_vibration_enabled); CHECK(!app_heat_authorized);
    app_vibration.calibration_ready=true; tick(321U); CHECK(app_heat_authorized);
    app_vibration.calibration_ready=false; tick(322U); CHECK(!app_heat_authorized);
    app_vibration.calibration_retained=true; tick(323U); CHECK(!app_heat_authorized);
    app_vibration.calibration_ready=true; tick(324U); CHECK(app_heat_authorized);
    CHECK(app_heat_current==HEATER_AVERAGE_CURRENT_NORMAL_MA);
    app_charge.cool=true; app_charge.ntc_fresh=true; tick(325U);
    CHECK(app_heat_current==HEATER_AVERAGE_CURRENT_REDUCED_MA);
    app_charge.ntc_fresh=false; tick(326U); CHECK(app_heat_current==HEATER_AVERAGE_CURRENT_NORMAL_MA);
    app_sensors.tip_mdegc=9999; tick(327U); CHECK(app_heat_current==HEATER_AVERAGE_CURRENT_REDUCED_MA);
    app_sensors.tip_mdegc=10000; tick(328U); CHECK(app_heat_current==HEATER_AVERAGE_CURRENT_NORMAL_MA);
    reset(0U); ready(); app_settings.vibration_level=0U; raw_power(true);
    tick(0U); tick(20U); tick(320U);
    CHECK(state()==APP_NORMAL); CHECK(app_heat_authorized); CHECK(app_vibration_level==0U);
}
static void test_low_battery_notice_input_loss_and_no_resume(void) {
    normal(0U); app_charge.input_valid=true; app_charge.active_charging=true;
    app_sensors.battery_mv=BATTERY_STANDBY_MV; tick(400U);
    CHECK(state()==APP_LOW_BATTERY_NOTICE); CHECK(app_power==POWER_STATE_READY);
    CHECK(!app_heat_authorized); CHECK(!app_vibration_enabled); CHECK(app_prepare_sleep==0U);
    app_charge.input_valid=false; tick(401U);
    CHECK(app_power==POWER_STATE_DISABLED); CHECK(state()==APP_LOW_BATTERY_NOTICE);
    CHECK(app_prepare_sleep==0U); /* Continue polling through notice. */
    tick(3400U); CHECK(state()==APP_SLEEP);
    reset(0U); ready(); app_charge.input_valid=true; app_charge.active_charging=true;
    app_sensors.battery_mv=BATTERY_STANDBY_MV; raw_power(true); tick(0U); tick(20U); tick(320U);
    CHECK(state()==APP_CHARGING); app_sensors.battery_mv=BATTERY_STANDBY_MV+1U;
    tick(1000U); CHECK(state()==APP_CHARGING); /* Rejected held press cannot revive. */
    release_power(1100U); raw_power(true); tick(1200U); tick(1220U); tick(1520U);
    CHECK(state()==APP_CHARGING_NORMAL);
}
static void test_recovery_deadlines_and_shipping_equality(void) {
    uint32_t base=UINT32_MAX-5000U;
    reset(base); ready(); app_sensors.battery_mv=BATTERY_DISCONNECT_MV;
    app_charge.input_valid=true; tick(base); tick(base+20U); CHECK(state()==APP_CHARGING_RECOVERY);
    CHECK(app_power==POWER_STATE_READY); CHECK(app_shipping==0U);
    tick(base+9999U); CHECK(state()==APP_CHARGING_RECOVERY);
    app_charge.active_charging=true; tick(base+10000U); CHECK(state()==APP_CHARGING);
    tick(base+20000U); CHECK(app_shipping==0U);
    app_charge.active_charging=false; tick(base+20001U); CHECK(state()==APP_CHARGING_RECOVERY);
    tick(base+30000U); CHECK(app_shipping==0U);
    tick(base+30001U); CHECK(state()==APP_BATTERY_DISCONNECT); CHECK(app_shipping==1U);
    tick(base+30002U); CHECK(app_shipping==1U);
    reset(0U); ready(); app_sensors.battery_mv=BATTERY_STANDBY_MV;
    app_charge.input_valid=true; tick(0U); tick(20U); tick(10000U);
    CHECK(state()==APP_LOW_BATTERY_NOTICE); CHECK(app_power==POWER_STATE_DISABLED);
    tick(13000U); CHECK(state()==APP_SLEEP);
    reset(0U); ready(); app_sensors.battery_mv=BATTERY_DISCONNECT_MV;
    app_charge.input_valid=true; tick(20U); app_charge.input_valid=false; tick(21U);
    CHECK(state()==APP_BATTERY_DISCONNECT); CHECK(app_shipping==1U);
    reset(0U); ready(); app_sensors.battery_mv=BATTERY_DISCONNECT_MV; tick(20U);
    CHECK(state()==APP_BATTERY_DISCONNECT);
}
static void test_recovery_recovers_without_activation(void) {
    reset(0U); ready(); app_sensors.battery_mv=BATTERY_STANDBY_MV;
    app_charge.input_valid=true; tick(0U); tick(20U); CHECK(state()==APP_CHARGING_RECOVERY);
    raw_power(true); tick(1U); tick(21U); tick(321U);
    app_sensors.battery_mv=BATTERY_STANDBY_MV+1U; tick(1000U);
    CHECK(state()==APP_CHARGING); CHECK(!app_heat_authorized);
    release_power(1100U); raw_power(true); tick(1200U); tick(1220U); tick(1520U);
    CHECK(state()==APP_CHARGING_NORMAL);
    app_sensors.battery_mv=BATTERY_STANDBY_MV; app_charge.active_charging=false; tick(1600U);
    CHECK(state()==APP_LOW_BATTERY_NOTICE); tick(4600U); CHECK(state()==APP_CHARGING_RECOVERY);
    app_charge.input_valid=false; tick(4601U); CHECK(app_power==POWER_STATE_DISABLED);
    CHECK(state()==APP_LOW_BATTERY_NOTICE);
}
static void inject_fault(unsigned code) {
    switch(code) {
    case 1: app_sensors.tip_mdegc=HEATER_FAULT_TEMP_MDEGC; break;
    case 2: app_sensors.invalid_tip=true; break;
    case 3: app_sensors.invalid_battery=true; break;
    case 4: app_power=POWER_STATE_FAILURE; break;
    case 5: app_power=POWER_STATE_FAILURE; break;
    case 6: app_charge.hot=true; break;
    case 7: app_charge.cold=true; break;
    case 8: app_charge.charger_fault=true; break;
    case 9: app_vibration.driver_fault=true; break;
    case 10: app_charge.communication_fault=true; break;
    case 11: app_vibration.communication_fault=true; break;
    case 12: app_sensors.acquisition_fault=true; break;
    case 13: app_vibration.calibration_fault=true; break;
    }
}
static void test_all_fault_codes_priority_and_first_latch(void) {
    for (unsigned code=1U; code<=13U; ++code) {
        if (code==4U) reset(0U); else normal(0U);
        inject_fault(code); tick(400U);
        CHECK(state()==APP_FAULT_DISPLAY); CHECK(App_GetSnapshot().fault_code==code);
        CHECK(app_power==POWER_STATE_DISABLED); CHECK(!app_heat_authorized); CHECK(!app_vibration_enabled);
        inject_fault(1U); tick(401U); CHECK(App_GetSnapshot().fault_code==code);
    }
    /* All simultaneous pairs establish the specified code order. */
    for (unsigned first=1U; first<13U; ++first) {
        for (unsigned second=first+1U; second<=13U; ++second) {
            if (first==4U) reset(0U); else normal(0U);
            /* PG timeout/loss are mutually exclusive observations. */
            if (second==4U || (first==4U && second==5U)) continue;
            inject_fault(second); inject_fault(first); tick(400U);
            CHECK(App_GetSnapshot().fault_code==first);
        }
    }
    normal(0U); app_charge.watchdog_fault=true; tick(400U);
    CHECK(App_GetSnapshot().fault_code==8U);
}
static void test_fault_sleep_wake_no_sensing_and_reboot(void) {
    uint32_t base=UINT32_MAX-1000U;
    normal(base); app_charge.cold=true; tick(base+400U);
    tick(base+400U+APP_FAULT_INITIAL_DISPLAY_MS-1U); CHECK(state()==APP_FAULT_DISPLAY);
    tick(base+400U+APP_FAULT_INITIAL_DISPLAY_MS); CHECK(state()==APP_FAULT_SLEEP);
    unsigned loads=app_loads, validations=app_validations;
    unsigned prepared=app_prepare_sleep;
    CHECK(prepared==1U);
    App_Wake(base+61000U, APP_WAKE_CHARGER, false, 0U); tick(base+61000U);
    CHECK(state()==APP_FAULT_DISPLAY); CHECK(!app_sensing_power); CHECK(app_power==POWER_STATE_DISABLED);
    CHECK(app_loads==loads); CHECK(app_validations==validations);
    CHECK(app_prepare_sleep==prepared);
    tick(base+71000U); CHECK(state()==APP_FAULT_SLEEP);
    raw_power(true); App_Wake(base+72000U, APP_WAKE_POWER, true, base+72000U);
    tick(base+73000U); CHECK(state()==APP_FAULT_DISPLAY); CHECK(!app_sensing_power);
    tick(base+82000U); CHECK(state()==APP_FAULT_SLEEP);
    /* 10 s hardware reset is not emulated: only explicit reboot clears latch. */
    reset(0U); CHECK(App_GetSnapshot().fault_code==0U); CHECK(state()==APP_STARTUP);
}
static void test_status_ack_after_fault_consumption(void) {
    normal(0U); app_vibration.driver_fault=true;
    app_vibration.status=(DRV2624_StatusSnapshot){true, VIBRATION_ERROR_MASK, 5U};
    tick(400U); CHECK(App_GetSnapshot().fault_code==9U); CHECK(app_status_acks==1U);
}
static void test_notice_usb_reinsertion_reselects_recovery(void) {
    normal(0U); app_sensors.battery_mv=BATTERY_STANDBY_MV; tick(400U);
    CHECK(state()==APP_LOW_BATTERY_NOTICE); CHECK(app_power==POWER_STATE_DISABLED);
    app_charge.input_valid=true; app_charge.active_charging=false; tick(500U);
    CHECK(app_power==POWER_STATE_STARTING); CHECK(app_prepare_sleep==0U);
    app_power=POWER_STATE_READY; tick(3400U);
    CHECK(state()==APP_CHARGING_RECOVERY);
    tick(10499U); CHECK(state()==APP_CHARGING_RECOVERY);
    tick(10500U); CHECK(state()==APP_LOW_BATTERY_NOTICE);
}
static void test_session_deadline_rollover(void) {
    uint32_t base=UINT32_MAX-1000U;
    normal(base); uint32_t start=App_GetSnapshot().session_started_ms;
    tick(start+APP_SESSION_LIMIT_MS-1U); CHECK(state()==APP_NORMAL);
    tick(start+APP_SESSION_LIMIT_MS); CHECK(state()==APP_SLEEP);
}
static void test_sleep_readiness_waits_for_all_owners(void) {
    reset(0U); ready(); app_sensor_stop_ready=false; tick(20U);
    CHECK(state()==APP_SLEEP); CHECK(App_GetSnapshot().sleep_requested);
    CHECK(App_GetSnapshot().shutdown_pending); CHECK(!App_GetSnapshot().sleep_ready);
    app_charge.sleep_ready=true;
    tick(21U); CHECK(!App_GetSnapshot().sleep_ready); /* ADC alone still owns hardware. */
    app_sensor_stop_ready=true; tick(22U); CHECK(App_GetSnapshot().sleep_ready);
    app_charge.sleep_ready=false; CHECK(!App_GetSnapshot().sleep_ready);
    app_charge.sleep_ready=true; app_charge.recovering=true;
    CHECK(!App_GetSnapshot().sleep_ready);
    app_charge.recovering=false; app_drv_result=(I2C_DeviceResult){.state=I2C_RESULT_PENDING};
    CHECK(!App_GetSnapshot().sleep_ready);
    app_drv_result=(I2C_DeviceResult){.state=I2C_RESULT_CANCELLED, .recovering=true};
    CHECK(!App_GetSnapshot().sleep_ready);
    app_drv_result=(I2C_DeviceResult){.state=I2C_RESULT_IDLE};
    app_heater.active=true; CHECK(!App_GetSnapshot().sleep_ready);
    app_heater.active=false; app_heater.duty_ppm=1U; CHECK(!App_GetSnapshot().sleep_ready);
    app_heater.duty_ppm=0U; app_vibration.output_active=true; CHECK(!App_GetSnapshot().sleep_ready);
    app_vibration.output_active=false;
    CHECK(App_GetSnapshot().sleep_ready); CHECK(!App_GetSnapshot().shutdown_pending);
    App_Wake(30U, APP_WAKE_CHARGER, false, 0U); CHECK(!App_GetSnapshot().sleep_requested);
}
static void test_explicit_initialization_bindings(void) {
    ADC_HandleTypeDef adc={0}; I2C_HandleTypeDef charger={0}, driver={0};
    Sensors_AcquisitionOps sensors={0}; I2C_DeviceOps charger_ops={0}, driver_ops={0};
    Charging_Profile charging={0}; Vibration_Profile vibration={0}; Heater_PwmOps pwm={0};
    int context=0;
    App_Bindings bindings={.adc=&adc, .sensors=&sensors, .charger_i2c=&charger,
        .vibration_i2c=&driver, .charger_transport=&charger_ops, .vibration_transport=&driver_ops,
        .charging=&charging, .vibration=&vibration, .heater_pwm=&pwm, .heater_context=&context};
    reset(0U); App_Init(&bindings,0U);
    CHECK(app_received_bindings.adc==&adc); CHECK(app_received_bindings.sensors==&sensors);
    CHECK(app_received_bindings.charger_i2c==&charger); CHECK(app_received_bindings.vibration_i2c==&driver);
    CHECK(app_received_bindings.charger_transport==&charger_ops); CHECK(app_received_bindings.vibration_transport==&driver_ops);
    CHECK(app_received_bindings.charging==&charging); CHECK(app_received_bindings.vibration==&vibration);
    CHECK(app_received_bindings.heater_pwm==&pwm); CHECK(app_received_bindings.heater_context==&context);
}
static void test_heat_only_requires_verified_driver_entry(void) {
    reset(0U); ready(); app_settings.vibration_level=0U;
    app_vibration.configuration_ready=false; raw_power(true); tick(0U); tick(20U); tick(320U);
    CHECK(state()==APP_NORMAL); CHECK(!app_heat_authorized); CHECK(app_vibration_level==0U);
    app_vibration.configuration_ready=true; tick(321U); CHECK(app_heat_authorized);
    CHECK(!app_vibration.calibration_ready);
}
static void test_faults_during_shutdown_drain_and_shipping(void) {
    normal(0U); release_power(400U); raw_power(true); tick(500U); tick(520U); tick(1520U);
    CHECK(state()==APP_SLEEP); CHECK(App_GetSnapshot().shutdown_pending);
    app_charge.communication_fault=true; tick(1521U);
    CHECK(state()==APP_FAULT_DISPLAY); CHECK(App_GetSnapshot().fault_code==10U);
    reset(0U); ready(); app_sensors.battery_mv=BATTERY_DISCONNECT_MV; tick(20U);
    CHECK(state()==APP_BATTERY_DISCONNECT);
    app_charge.communication_fault=true; tick(21U);
    CHECK(state()==APP_FAULT_DISPLAY); CHECK(App_GetSnapshot().fault_code==10U);
}
static void test_raw_waking_press_finishes_debounce_before_sleep(void) {
    reset(0U); ready(); tick(20U); CHECK(state()==APP_SLEEP);
    raw_power(true); App_Wake(100U, APP_WAKE_POWER, false, 0U); app_power=POWER_STATE_READY;
    tick(100U); CHECK(state()==APP_WAKEUP);
    tick(119U); CHECK(state()==APP_WAKEUP);
    tick(120U); tick(419U); CHECK(state()==APP_WAKEUP);
    tick(420U); CHECK(state()==APP_NORMAL);
    tick(1120U); CHECK(state()==APP_NORMAL);
}
static void test_inference_boundary_and_real_fault_low_priority(void) {
    reset(0U); ready(); app_charge.input_valid=true;
    app_sensors.tip_mdegc=HEATER_FAULT_TEMP_MDEGC-APP_COLD_INFERENCE_MARGIN_MDEGC-1L;
    tick(20U); CHECK(App_GetSnapshot().heat_level==1U);
    reset(0U); ready(); app_sensors.battery_mv=BATTERY_STANDBY_MV;
    app_charge.input_valid=true; app_sensors.tip_mdegc=HEATER_FAULT_TEMP_MDEGC;
    tick(20U); CHECK(state()==APP_FAULT_DISPLAY); CHECK(App_GetSnapshot().fault_code==1U);
}
static void test_fault_display_and_notice_led_commands(void) {
    normal(0U); app_charge.input_valid=true;
    app_sensors.battery_mv=BATTERY_STANDBY_MV; tick(400U);
    CHECK(state()==APP_LOW_BATTERY_NOTICE);
    CHECK(FakeHAL_GetOutput(BAT_LED_R_GPIO_Port,BAT_LED_R_Pin)==GPIO_PIN_SET);
    CHECK(FakeHAL_GetOutput(LED_HEAT_1_GPIO_Port,LED_HEAT_1_Pin)==GPIO_PIN_RESET);
    app_charge.hot=true; tick(401U); CHECK(App_GetSnapshot().fault_code==6U);
    CHECK(FakeHAL_GetOutput(LED_HEAT_1_GPIO_Port,LED_HEAT_1_Pin)==GPIO_PIN_RESET);
    CHECK(FakeHAL_GetOutput(LED_HEAT_2_GPIO_Port,LED_HEAT_2_Pin)==GPIO_PIN_SET);
    CHECK(FakeHAL_GetOutput(LED_HEAT_3_GPIO_Port,LED_HEAT_3_Pin)==GPIO_PIN_SET);
    CHECK(FakeHAL_GetOutput(LED_VIBRATION_1_GPIO_Port,LED_VIBRATION_1_Pin)==GPIO_PIN_RESET);
    tick(501U); CHECK(FakeHAL_GetOutput(BAT_LED_R_GPIO_Port,BAT_LED_R_Pin)==GPIO_PIN_RESET);
}
static void test_rejected_enable_finishes_validation(void) {
    reset(0U); ready(); app_charge.input_valid=true; app_vibration.profile_valid=false;
    raw_power(true); tick(0U); tick(20U); tick(320U);
    CHECK(state()==APP_CHARGING); CHECK(!app_vibration_enabled); CHECK(!app_heat_authorized);
    app_vibration.profile_valid=true; tick(1000U); CHECK(state()==APP_CHARGING);
    reset(0U); ready(); app_vibration.profile_valid=false;
    raw_power(true); tick(0U); tick(20U); tick(320U);
    CHECK(state()==APP_SLEEP); CHECK(!app_vibration_enabled);
}
static uint32_t pending_validation(bool wake, uint32_t battery_mv, bool input) {
    uint32_t base=UINT32_MAX-5000U;
    reset(base); ready();
    if (wake) {
        tick(base+20U); CHECK(state()==APP_SLEEP);
        base+=40U; App_Wake(base, APP_WAKE_CHARGER, false, 0U); app_power=POWER_STATE_READY;
    }
    app_sensors.battery_mv=battery_mv;
    app_charge.input_valid=input; app_charge.configuration_ready=false;
    return base;
}
static void test_low_battery_shutdown_precedes_pending_configuration(void) {
    for (unsigned wake=0U; wake<2U; ++wake) {
        uint32_t base=pending_validation(wake!=0U,BATTERY_DISCONNECT_MV,false);
        tick(base); CHECK(state()==APP_BATTERY_DISCONNECT); CHECK(app_shipping==1U);
        base=pending_validation(wake!=0U,BATTERY_STANDBY_MV,false);
        tick(base); CHECK(state()==APP_LOW_BATTERY_NOTICE); CHECK(app_power==POWER_STATE_DISABLED);
        base=pending_validation(wake!=0U,BATTERY_DISCONNECT_MV,true);
        tick(base); CHECK(state()==(wake ? APP_WAKEUP : APP_STARTUP));
        tick(base+APP_CHARGING_START_LIMIT_MS-1U); CHECK(app_shipping==0U);
        tick(base+APP_CHARGING_START_LIMIT_MS); CHECK(state()==APP_BATTERY_DISCONNECT);
        CHECK(app_shipping==1U);
        base=pending_validation(wake!=0U,BATTERY_DISCONNECT_MV,true);
        tick(base); app_charge.input_valid=false; tick(base+5000U);
        CHECK(state()==APP_BATTERY_DISCONNECT); CHECK(app_shipping==1U);
        base=pending_validation(wake!=0U,BATTERY_DISCONNECT_MV,false);
        app_charge.hot=true; tick(base);
        CHECK(state()==APP_FAULT_DISPLAY); CHECK(App_GetSnapshot().fault_code==6U);
        CHECK(app_shipping==0U);
    }
}
static void test_pending_configuration_active_phase_cancels_deadline(void) {
    for (unsigned wake=0U; wake<2U; ++wake) {
        uint32_t base=pending_validation(wake!=0U,BATTERY_STANDBY_MV,true);
        tick(base); app_charge.active_charging=true;
        tick(base+APP_CHARGING_START_LIMIT_MS);
        CHECK(state()==(wake ? APP_WAKEUP : APP_STARTUP)); CHECK(app_shipping==0U);
        tick(base+20000U); CHECK(app_power==POWER_STATE_READY);
        app_charge.active_charging=false; tick(base+20001U);
        tick(base+30000U); CHECK(state()==(wake ? APP_WAKEUP : APP_STARTUP));
        tick(base+30001U); CHECK(state()==APP_LOW_BATTERY_NOTICE);
        CHECK(app_power==POWER_STATE_DISABLED); CHECK(app_shipping==0U);
    }
}
static void test_pending_configuration_low_press_cannot_revive_on_recovery(void) {
    uint32_t base=pending_validation(false,BATTERY_STANDBY_MV,true);
    raw_power(true); tick(base); tick(base+20U); tick(base+320U);
    app_sensors.battery_mv=BATTERY_STANDBY_MV+1U;
    app_charge.configuration_ready=true; tick(base+500U);
    CHECK(state()!=APP_NORMAL && state()!=APP_CHARGING_NORMAL);
    CHECK(!app_heat_authorized && !app_vibration_enabled);
    release_power(base+600U); CHECK(state()==APP_CHARGING);
    raw_power(true); tick(base+700U); tick(base+720U); tick(base+1020U);
    CHECK(state()==APP_CHARGING_NORMAL);
}
static void test_raw_battery_admission_and_undervoltage(void) {
    for (unsigned operation=0U; operation<2U; ++operation) {
        for (unsigned usb=0U; usb<2U; ++usb) {
            for (uint32_t mv=2499U; mv<=2501U; ++mv) {
                if (operation) normal(0U); else { reset(0U); ready(); }
                app_charge.input_valid=usb!=0U;
                app_sensors.battery_mv=mv; app_sensors.filtered_battery_mv=4000U;
                app_sensors.fresh=false; /* Battery completion does not require tip. */
                tick(400U);
                CHECK(App_GetSnapshot().fault_code==(mv<2500U ? APP_FAULT_BATTERY_UNDERVOLTAGE : 0U));
                if (mv<2500U) {
                    CHECK(!App_GetSnapshot().charging_eligible);
                    CHECK(app_shipping==0U && !app_heat_authorized && !app_vibration_enabled);
                    app_sensors.battery_mv=4000U; app_charge.input_valid=!usb; tick(401U);
                    CHECK(App_GetSnapshot().fault_code==APP_FAULT_BATTERY_UNDERVOLTAGE);
                    tick(400U+APP_FAULT_INITIAL_DISPLAY_MS); CHECK(state()==APP_FAULT_SLEEP);
                    App_Wake(61000U,APP_WAKE_CHARGER,false,0U); tick(61000U);
                    CHECK(App_GetSnapshot().fault_code==APP_FAULT_BATTERY_UNDERVOLTAGE);
                    reset(0U); ready(); app_sensors.battery_mv=2499U; tick(1U);
                    CHECK(App_GetSnapshot().fault_code==APP_FAULT_BATTERY_UNDERVOLTAGE);
                } else if (usb) CHECK(App_GetSnapshot().charging_eligible);
            }
        }
    }
    normal(0U); app_charge.input_valid=true;
    app_sensors.battery_fresh=false; app_sensors.battery_current_valid=false;
    tick(400U); CHECK(!App_GetSnapshot().charging_eligible && App_GetSnapshot().fault_code==0U);
    app_sensors.acquisition_fault=true; tick(401U); CHECK(App_GetSnapshot().fault_code==12U);
    reset(0U); ready(); app_sensors.battery_mv=2499U;
    app_sensors.battery_current_valid=false; tick(1U);
    CHECK(App_GetSnapshot().fault_code==0U && !App_GetSnapshot().charging_eligible);
    app_sensors.battery_current_valid=false; app_sensors.invalid_battery=true;
    tick(1U); CHECK(App_GetSnapshot().fault_code==3U);
    reset(0U); ready(); app_sensors.battery_mv=2499U; app_power=POWER_STATE_FAILURE;
    tick(1U); CHECK(App_GetSnapshot().fault_code==APP_FAULT_BATTERY_UNDERVOLTAGE);
    reset(0U); ready(); app_sensors.battery_mv=2499U; app_sensors.invalid_tip=true;
    tick(1U); CHECK(App_GetSnapshot().fault_code==2U);
}

int main(void) {
    test_raw_battery_admission_and_undervoltage();
    test_raw_power_wake_rearms_cached_hold_and_debounces();
    test_charger_wake_keeps_continuing_shutdown_hold_consumed();
    test_verified_wake_after_sleeping_release_repress();
    test_pending_configuration_low_press_cannot_revive_on_recovery();
    test_low_battery_shutdown_precedes_pending_configuration();
    test_pending_configuration_active_phase_cancels_deadline();
    test_rejected_enable_finishes_validation();
    test_raw_waking_press_finishes_debounce_before_sleep();
    test_inference_boundary_and_real_fault_low_priority(); test_fault_display_and_notice_led_commands();
    test_faults_during_shutdown_drain_and_shipping();
    test_sleep_readiness_waits_for_all_owners(); test_explicit_initialization_bindings();
    test_heat_only_requires_verified_driver_entry();
    test_notice_usb_reinsertion_reselects_recovery(); test_session_deadline_rollover();
    test_startup_and_readiness(); test_waking_press_validation_and_rollover();
    test_short_wake_and_release_qualified_pending(); test_cold_only_inference_and_low_precedence();
    test_usb_session_and_gestures(); test_zero_timer_rollover_and_disable();
    test_heat_calibration_and_live_current(); test_low_battery_notice_input_loss_and_no_resume();
    test_recovery_deadlines_and_shipping_equality(); test_recovery_recovers_without_activation();
    test_all_fault_codes_priority_and_first_latch(); test_fault_sleep_wake_no_sensing_and_reboot();
    test_status_ack_after_fault_consumption();
    puts("app policy tests passed (30 groups, fault code pairs, wake episodes, rollover)");
    return 0;
}
