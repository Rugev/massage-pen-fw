#include "app.h"
#include "buttons.h"
#include "leds.h"
#include "power.h"
#include "storage.h"
#include "mp2724.h"
#include <stddef.h>

static App_Snapshot app;
static bool settings_loaded, enable_pending, power_was_ready;
static uint32_t consumed_press_id, state_started_ms, fault_display_ms;
static bool zero_timer_running, charge_timer_running, charge_input_before;
static uint32_t zero_started_ms, charge_started_ms;
static bool notice_shutdown;

static bool normal_operation(void)
{
    return app.state == APP_NORMAL || app.state == APP_CHARGING_NORMAL;
}
static bool validation(void)
{
    return app.state == APP_STARTUP || app.state == APP_WAKEUP;
}
static bool fault_state(void)
{
    return app.state == APP_FAULT_DISPLAY || app.state == APP_FAULT_SLEEP;
}
static void disable_power(void)
{
    Power_Disable();
    power_was_ready = false;
}
static void discard_enable(void)
{
    enable_pending = false;
    /* A held/released request cannot be reused after any session ending. */
    consumed_press_id = Buttons_PressIdentity(BUTTONS_POWER);
}
static void enter_state(App_State next, uint32_t now)
{
    if (next == app.state) return;
    bool fault_wake = app.state == APP_FAULT_SLEEP && next == APP_FAULT_DISPLAY;
    app.state = next;
    state_started_ms = now;
    if (next != APP_NORMAL && next != APP_CHARGING_NORMAL &&
        next != APP_STARTUP && next != APP_WAKEUP) discard_enable();
    if (next == APP_SLEEP || next == APP_FAULT_SLEEP) {
        disable_power();
        if (next == APP_SLEEP) Charging_RequestPrepareSleep();
    } else if (next == APP_FAULT_DISPLAY || next == APP_BATTERY_DISCONNECT) {
        disable_power();
        if (next == APP_FAULT_DISPLAY && !fault_wake) Charging_RequestPrepareSleep();
        else if (next == APP_BATTERY_DISCONNECT) (void)Charging_RequestShipping();
    }
}
static void begin_validation(App_State next, uint32_t now)
{
    app.state = next;
    state_started_ms = now;
    settings_loaded = false;
    enable_pending = false;
    power_was_ready = false;
    charge_timer_running = false;
    charge_input_before = false;
    Power_Enable();
    Charging_BeginValidation();
}
void App_Init(const App_Bindings *bindings, uint32_t now)
{
    const App_Bindings empty = {0};
    if (bindings == NULL) bindings = &empty;
    app = (App_Snapshot){.state = APP_STARTUP};
    consumed_press_id = 0U;
    zero_timer_running = false;
    notice_shutdown = false;
    fault_display_ms = APP_FAULT_INITIAL_DISPLAY_MS;
    Power_Init();
    Buttons_Init(now);
    Leds_Init(now);
    Sensors_Init(bindings->adc, bindings->sensors);
    MP2724_Init(bindings->charger_i2c, bindings->charger_transport);
    DRV2624_Init(bindings->vibration_i2c, bindings->vibration_transport);
    Charging_Init(bindings->charging);
    Vibration_Init(bindings->vibration);
    Heater_Init(bindings->heater_pwm, bindings->heater_context);
    begin_validation(APP_STARTUP, now);
}
void App_Wake(uint32_t now, App_WakeSource source, bool verified_press, uint32_t press_started)
{
    if (app.state == APP_FAULT_SLEEP) {
        fault_display_ms = APP_FAULT_WAKE_DISPLAY_MS;
        enter_state(APP_FAULT_DISPLAY, now);
        return;
    }
    if (app.state != APP_SLEEP) return;
    begin_validation(APP_WAKEUP, now);
    if (source == APP_WAKE_POWER) {
        if (verified_press) Buttons_AdoptWakePress(BUTTONS_POWER, press_started);
        else Buttons_RearmWakePress(BUTTONS_POWER, now);
    }
    Buttons_Update(now);
}
App_Snapshot App_GetSnapshot(void)
{
    App_Snapshot snapshot = app;
    snapshot.sleep_requested = app.state == APP_SLEEP || app.state == APP_FAULT_SLEEP;
    Charging_Observation charging = Charging_GetObservation();
    I2C_DeviceResult driver = DRV2624_GetResult();
    Heater_Snapshot heater = Heater_GetSnapshot();
    snapshot.sleep_ready = snapshot.sleep_requested && Power_GetState() == POWER_STATE_DISABLED &&
        Sensors_IsQuiescent() && charging.sleep_ready && !charging.recovering &&
        driver.state != I2C_RESULT_PENDING && !driver.recovering &&
        !Vibration_GetObservation().output_active && !heater.active && heater.duty_ppm == 0U;
    snapshot.shutdown_pending = snapshot.sleep_requested && !snapshot.sleep_ready;
    return snapshot;
}

static uint8_t detected_fault(Power_State_t power, const Sensors_Snapshot *s,
                              const Charging_Observation *c,
                              const Vibration_Observation *v)
{
    /* Evaluate all observations before acknowledging shared read-to-clear status.
     * This order is the agreed simultaneous-fault priority, not detection order. */
    if (Heater_GetSnapshot().fault_temperature ||
        (s->fresh && s->tip_valid && s->tip_mdegc >= HEATER_FAULT_TEMP_MDEGC)) return 1U;
    if (s->invalid_tip) return 2U;
    if (s->invalid_battery) return 3U;
    if (s->battery_fresh && s->battery_current_valid &&
        s->battery_mv < BATTERY_CHARGE_ADMISSION_MIN_MV) return APP_FAULT_BATTERY_UNDERVOLTAGE;
    if (power == POWER_STATE_FAILURE) return power_was_ready ? 5U : 4U;
    if (c->hot) return 6U;
    if (c->cold) return 7U;
    if (c->charger_fault || c->watchdog_fault) return 8U;
    if (v->driver_fault) return 9U;
    if (c->communication_fault) return 10U;
    if (v->communication_fault) return 11U;
    if (s->acquisition_fault) return 12U;
    if (v->calibration_fault) return 13U;
    return 0U;
}
static void latch_fault(uint8_t code, uint32_t now)
{
    if (app.fault_code != 0U || code == 0U) return;
    app.fault_code = code;
    fault_display_ms = APP_FAULT_INITIAL_DISPLAY_MS;
    enter_state(APP_FAULT_DISPLAY, now);
}
static void cycle_click(Buttons_Id id, const Buttons_Event *event)
{
    if (!normal_operation() || !(event->flags & BUTTONS_EVENT_RELEASED) ||
        event->duration_ms <= APP_CLICK_MIN_MS || event->duration_ms >= APP_CLICK_MAX_MS) return;
    if (id == BUTTONS_HEAT)
        app.heat_level = (uint8_t)((app.heat_level + 1U) % HEATER_LEVEL_COUNT);
    else if (id == BUTTONS_VIBRATION)
        app.vibration_level = (uint8_t)((app.vibration_level + 1U) % VIBRATION_LEVEL_COUNT);
}
static bool handle_buttons(uint32_t now)
{
    Buttons_Event power_event = {0};
    for (unsigned i = 0; i < BUTTONS_COUNT; ++i) {
        Buttons_Event event = {0};
        if (!Buttons_TakeEvent((Buttons_Id)i, &event)) continue;
        if (i == BUTTONS_POWER) power_event = event;
        else cycle_click((Buttons_Id)i, &event);
    }
    if (fault_state() || app.state == APP_BATTERY_DISCONNECT) return false;
    uint32_t identity = Buttons_PressIdentity(BUTTONS_POWER);
    bool held = Buttons_IsPressed(BUTTONS_POWER);
    uint32_t duration = held ? Buttons_PressDuration(BUTTONS_POWER, now) : power_event.duration_ms;
    if (identity == 0U || identity == consumed_press_id) return false;
    if (normal_operation()) {
        if (held && duration >= APP_DISABLE_HOLD_MS) {
            consumed_press_id = identity;
            return true;
        }
    } else if ((held || (power_event.flags & BUTTONS_EVENT_RELEASED)) &&
               duration >= APP_ENABLE_HOLD_MS) {
        consumed_press_id = identity;
        /* Notice/sleep/recovery may consume a gesture but cannot queue it for
         * a later recovered voltage. Only validation/Charging accept enables. */
        if (validation() || app.state == APP_CHARGING) enable_pending = true;
    }
    return false;
}
static bool session_expired(uint32_t now)
{
    if (!normal_operation()) {
        zero_timer_running = false;
        return false;
    }
    if (app.heat_level == 0U && app.vibration_level == 0U) {
        if (!zero_timer_running) {
            zero_timer_running = true;
            zero_started_ms = now;
        }
    } else zero_timer_running = false;
    return (uint32_t)(now - app.session_started_ms) >= APP_SESSION_LIMIT_MS ||
        (zero_timer_running && (uint32_t)(now - zero_started_ms) > APP_ZERO_LEVEL_LIMIT_MS);
}
static bool low_battery(const Sensors_Snapshot *s)
{
    return s->ready && s->battery_mv <= BATTERY_STANDBY_MV;
}
static void update_charge_deadline(uint32_t now, bool low, const Charging_Observation *c)
{
    if (!low || !c->input_valid || (c->active_charging && c->admitted)) charge_timer_running = false;
    else if (!charge_timer_running || !charge_input_before) {
        charge_timer_running = true;
        charge_started_ms = now;
    }
    charge_input_before = c->input_valid;
}
static bool charge_deadline_expired(uint32_t now)
{
    return charge_timer_running &&
        (uint32_t)(now - charge_started_ms) >= APP_CHARGING_START_LIMIT_MS;
}
static void begin_notice(uint32_t now, bool shutdown)
{
    notice_shutdown = shutdown;
    enter_state(APP_LOW_BATTERY_NOTICE, now);
    if (shutdown) disable_power();
}
static void apply_battery_shutdown(uint32_t now, const Sensors_Snapshot *s)
{
    if (s->battery_mv <= BATTERY_DISCONNECT_MV)
        enter_state(APP_BATTERY_DISCONNECT, now);
    else if (app.state != APP_LOW_BATTERY_NOTICE) begin_notice(now, true);
    else {
        notice_shutdown = true;
        disable_power();
    }
}
static bool can_enable(const Sensors_Snapshot *s, const Charging_Observation *c,
                       const Vibration_Observation *v)
{
    return Power_GetState() == POWER_STATE_READY && s->ready &&
        s->battery_mv > BATTERY_STANDBY_MV && c->configuration_ready && c->status_ready &&
        (app.vibration_level == 0U || v->profile_valid);
}
static void enable_session(uint32_t now, bool input)
{
    app.session_started_ms = now;
    zero_timer_running = false;
    enable_pending = false;
    enter_state(input ? APP_CHARGING_NORMAL : APP_NORMAL, now);
}
static void end_session(uint32_t now, bool input)
{
    enter_state(input ? APP_CHARGING : APP_SLEEP, now);
}
static void validation_policy(uint32_t now, const Sensors_Snapshot *s,
                              const Charging_Observation *c, const Vibration_Observation *v)
{
    if (Power_GetState() != POWER_STATE_READY || !s->ready || !c->status_ready) return;
    /* Once voltage and input status are known, shutdown cannot wait for an
     * unrelated configuration lease/readback. Fault observations were already
     * consumed this cycle. Keep validation open while recovery is still allowed. */
    if (low_battery(s)) discard_enable();
    if (low_battery(s) && (!c->input_valid || charge_deadline_expired(now))) {
        Charging_EndValidation();
        apply_battery_shutdown(now, s);
        return;
    }
    if (!c->configuration_ready || (uint32_t)(now - state_started_ms) < BUTTONS_DEBOUNCE_MS) return;
    bool cold_boot = app.state == APP_STARTUP;
    if (!settings_loaded) {
        Storage_Settings settings = Storage_Load();
        app.heat_level = settings.heat_level;
        app.vibration_level = settings.vibration_level;
        settings_loaded = true;
        if (cold_boot && !low_battery(s) &&
            s->tip_mdegc >= HEATER_FAULT_TEMP_MDEGC - APP_COLD_INFERENCE_MARGIN_MDEGC) {
            app.heat_level = 0U;
            app.vibration_level = 0U;
        }
    }
    if (low_battery(s)) {
        Charging_EndValidation();
        enter_state(c->active_charging ? APP_CHARGING : APP_CHARGING_RECOVERY, now);
        return;
    }
    if (enable_pending) {
        Charging_EndValidation();
        if (can_enable(s, c, v)) enable_session(now, c->input_valid);
        else enter_state(c->input_valid ? APP_CHARGING : APP_SLEEP, now);
    } else if (!Buttons_IsPressed(BUTTONS_POWER)) {
        Charging_EndValidation();
        enter_state(c->input_valid ? APP_CHARGING : APP_SLEEP, now);
    }
}
static void notice_policy(uint32_t now, const Sensors_Snapshot *s, const Charging_Observation *c)
{
    if (!c->input_valid || charge_deadline_expired(now)) {
        if (low_battery(s)) apply_battery_shutdown(now, s);
        else {
            notice_shutdown = true;
            disable_power();
        }
        if (app.state != APP_LOW_BATTERY_NOTICE) return;
    } else if (c->active_charging) notice_shutdown = false;
    if (!notice_shutdown && c->input_valid) Power_Enable();
    if (!Leds_NoticeComplete(now)) return;
    if (notice_shutdown || !c->input_valid) enter_state(APP_SLEEP, now);
    else enter_state(low_battery(s) && !c->active_charging ?
                     APP_CHARGING_RECOVERY : APP_CHARGING, now);
}
static void operating_policy(uint32_t now, bool shutdown, const Sensors_Snapshot *s,
                             const Charging_Observation *c, const Vibration_Observation *v)
{
    if (validation()) {
        validation_policy(now, s, c, v);
    } else if (normal_operation()) {
        if (low_battery(s)) {
            if (!c->input_valid || charge_deadline_expired(now)) apply_battery_shutdown(now, s);
            else begin_notice(now, false);
        } else if (shutdown) end_session(now, c->input_valid);
        else enter_state(c->input_valid ? APP_CHARGING_NORMAL : APP_NORMAL, now);
    } else if (app.state == APP_LOW_BATTERY_NOTICE) {
        notice_policy(now, s, c);
    } else if (app.state == APP_CHARGING || app.state == APP_CHARGING_RECOVERY) {
        if (low_battery(s)) {
            discard_enable();
            if (!c->input_valid || charge_deadline_expired(now)) apply_battery_shutdown(now, s);
            else enter_state(c->active_charging ? APP_CHARGING : APP_CHARGING_RECOVERY, now);
        } else if (!c->input_valid) enter_state(APP_SLEEP, now);
        else {
            enter_state(APP_CHARGING, now);
            if (enable_pending && can_enable(s, c, v)) enable_session(now, true);
        }
    }
}
static void display_policy(uint32_t now)
{
    Leds_Display display = {.mode = LEDS_OFF};
    if (app.state == APP_FAULT_DISPLAY) {
        display.mode = LEDS_FAULT;
        display.fault_code = app.fault_code;
    } else if (app.state == APP_LOW_BATTERY_NOTICE) display.mode = LEDS_LOW_BATTERY;
    else if (normal_operation()) {
        Heater_Phase phase = Heater_GetSnapshot().phase;
        display.mode = LEDS_LEVELS;
        display.heat_level = app.heat_level;
        display.vibration_level = app.vibration_level;
        display.heater_breathe = phase == HEATER_PREHEAT || phase == HEATER_PRECOOL;
    }
    /* Detailed charging-only LED policy remains a separate agreed topic. */
    Leds_Request(&display, now);
    Leds_Update(now);
}
static void publish_charge_policy(const Sensors_Snapshot *s)
{
    bool powered = Power_GetState() == POWER_STATE_READY && !fault_state();
    bool demand = !fault_state() && app.state != APP_SLEEP && app.state != APP_BATTERY_DISCONNECT;
    app.battery_sequence = s->battery_sequence;
    app.charging_eligible = powered && s->battery_fresh && s->battery_current_valid &&
        s->battery_mv >= BATTERY_CHARGE_ADMISSION_MIN_MV && demand;
    Charging_SetChargeRequired(demand);
    Charging_SetEligibility(app.charging_eligible, s->battery_sequence, normal_operation(), fault_state());
}
void App_Update(uint32_t now, bool charger_available)
{
    Power_Update();
    Power_State_t power = Power_GetState();
    if (power == POWER_STATE_READY) power_was_ready = true;
    Buttons_Update(now);
    bool previous_normal = normal_operation();
    bool sensing_power = power == POWER_STATE_READY && !fault_state();
    Sensors_Update(sensing_power);
    Sensors_Snapshot sensors = Sensors_GetSnapshot();
    Charging_Observation charging = Charging_GetObservation();
    Vibration_Update(sensing_power, previous_normal, app.vibration_level);
    Vibration_Observation vibration = Vibration_GetObservation();

    if (!fault_state() && (app.state != APP_SLEEP || !App_GetSnapshot().sleep_ready))
        latch_fault(detected_fault(power, &sensors, &charging, &vibration), now);
    bool user_shutdown = handle_buttons(now);
    if (app.state == APP_FAULT_DISPLAY) {
        if ((uint32_t)(now - state_started_ms) >= fault_display_ms)
            enter_state(APP_FAULT_SLEEP, now);
    } else if (!fault_state() && app.state != APP_BATTERY_DISCONNECT && app.state != APP_SLEEP) {
        bool input_inserted = charging.input_valid && !charge_input_before;
        update_charge_deadline(now, low_battery(&sensors), &charging);
        if (app.state == APP_LOW_BATTERY_NOTICE && input_inserted) notice_shutdown = false;
        operating_policy(now, user_shutdown || session_expired(now), &sensors, &charging, &vibration);
    }

    publish_charge_policy(&sensors);
    Charging_Update(charger_available);
    charging = Charging_GetObservation();
    if (!fault_state() && (app.state != APP_SLEEP || !App_GetSnapshot().sleep_ready))
        latch_fault(detected_fault(Power_GetState(), &sensors, &charging, &vibration), now);
    if (!fault_state() && app.state != APP_BATTERY_DISCONNECT && app.state != APP_SLEEP) {
        update_charge_deadline(now, low_battery(&sensors), &charging);
        operating_policy(now, false, &sensors, &charging, &vibration);
    }
    publish_charge_policy(&sensors);

    bool output_power = Power_GetState() == POWER_STATE_READY && !fault_state();
    bool enabled = normal_operation() && output_power;
    bool heat_authorized = enabled && vibration.configuration_ready &&
        (app.vibration_level == 0U || vibration.calibration_ready);
    uint32_t current = ((charging.ntc_fresh && charging.cool) ||
                        (sensors.ready && sensors.tip_mdegc < HEATER_REDUCED_CURRENT_TIP_MDEGC)) ?
        HEATER_AVERAGE_CURRENT_REDUCED_MA : HEATER_AVERAGE_CURRENT_NORMAL_MA;
    Heater_Update(now, app.heat_level, heat_authorized, current, &sensors);
    if (previous_normal != enabled || sensing_power != output_power)
        Vibration_Update(output_power, enabled, app.vibration_level);
    if (sensing_power && !output_power) Sensors_Update(false);
    if (vibration.status.valid) (void)DRV2624_AcknowledgeStatus(vibration.status.sequence);
    display_policy(now);
}
