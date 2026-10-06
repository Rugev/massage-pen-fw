#include "heater.h"
#include "pid.h"
#include <stddef.h>

static const Heater_PwmOps *pwm;
static void *pwm_context;
static Heater_Snapshot snapshot;
static PID_Controller controller;
static bool controls_valid;
static bool inhibited;
static uint8_t previous_level;
static uint32_t battery_mv;
static int32_t tip_mdegc;
static uint32_t last_control_ms;
static uint32_t requested_power_mw;

static int32_t level_target(uint8_t level)
{
    switch (level) {
    case 1U: return HEATER_LEVEL_1_TEMP_MDEGC;
    case 2U: return HEATER_LEVEL_2_TEMP_MDEGC;
    case 3U: return HEATER_LEVEL_3_TEMP_MDEGC;
    default: return HEATER_LEVEL_0_TEMP_MDEGC;
    }
}

static void write_duty(uint32_t duty)
{
    snapshot.duty_ppm = duty;
    if (snapshot.available) {
        pwm->set_duty(pwm_context, duty);
    }
}

static void stop_control(void)
{
    snapshot.active = false;
    snapshot.phase = HEATER_OFF;
    snapshot.power_mw = 0U;
    requested_power_mw = 0U;
    previous_level = 0U;
    PID_Reset(&controller);
    write_duty(0U);
}

static void enter_phase(Heater_Phase phase)
{
    snapshot.phase = phase;
    requested_power_mw = 0U;
    PID_Reset(&controller);
}

static Heater_Phase select_phase(void)
{
    if (tip_mdegc < snapshot.target_mdegc - HEATER_PHASE_BAND_MDEGC) {
        return HEATER_PREHEAT;
    }
    if (tip_mdegc > snapshot.target_mdegc + HEATER_PHASE_BAND_MDEGC) {
        return HEATER_PRECOOL;
    }
    return HEATER_PID;
}

static uint32_t maximum_duty(uint32_t current_ma)
{
    if (battery_mv == 0U) {
        return 0U;
    }
    uint64_t limit = (uint64_t)current_ma * HEATER_RESISTANCE_MOHM *
                     HEATER_DUTY_SCALE / ((uint64_t)battery_mv * HEATER_MA_PER_A);
    if (limit > HEATER_DUTY_SCALE) {
        limit = HEATER_DUTY_SCALE;
    }
    return (uint32_t)limit;
}

static uint32_t duty_power(uint32_t duty)
{
    return (uint32_t)((uint64_t)battery_mv * battery_mv * duty /
                     ((uint64_t)HEATER_RESISTANCE_MOHM * HEATER_DUTY_SCALE));
}

void Heater_Init(const Heater_PwmOps *ops, void *context)
{
    pwm = ops;
    pwm_context = context;
    snapshot = (Heater_Snapshot){0};
    snapshot.available = ops != NULL && ops->set_duty != NULL;
    controls_valid = false;
    inhibited = false;
    previous_level = 0U;
    battery_mv = 0U;
    tip_mdegc = 0;
    last_control_ms = 0U;
    requested_power_mw = 0U;
    PID_Reset(&controller);
    write_duty(0U);
}

void Heater_Update(uint32_t now_ms, uint8_t requested_level, bool authorized,
                   uint32_t average_current_limit_ma,
                   const Sensors_Snapshot *sensors)
{
    /* Protection uses every fresh valid raw reading, even while disabled or
     * between 50 ms control updates. The app owns the overall fault lifecycle. */
    if (sensors != NULL && sensors->fresh && sensors->tip_valid) {
        if (sensors->tip_mdegc >= HEATER_FAULT_TEMP_MDEGC) {
            snapshot.fault_temperature = true;
        }
        if (sensors->tip_mdegc >= HEATER_ABSOLUTE_MAX_TEMP_MDEGC) {
            inhibited = true;
        }
    }

    snapshot.target_mdegc = level_target(requested_level);
    bool sensors_ready = sensors != NULL && sensors->available && sensors->ready &&
                         !sensors->invalid_battery && !sensors->invalid_tip &&
                         !sensors->acquisition_fault;
    if (!sensors_ready) {
        controls_valid = false;
    } else if (sensors->battery_valid && sensors->tip_valid) {
        /* Missing acquisitions retain the most recent complete valid controls. */
        battery_mv = sensors->filtered_battery_mv;
        tip_mdegc = sensors->filtered_tip_mdegc;
        controls_valid = battery_mv <= SENSORS_BATTERY_MAX_MV;
    }
    if (!snapshot.available || !authorized || requested_level == 0U ||
        requested_level >= HEATER_LEVEL_COUNT || !controls_valid ||
        !sensors_ready || snapshot.fault_temperature) {
        stop_control();
        return;
    }

    bool resumed = false;
    if (inhibited && sensors->fresh && sensors->tip_valid &&
        sensors->tip_mdegc < snapshot.target_mdegc) {
        inhibited = false;
        resumed = true;
    }
    bool changed = !snapshot.active || requested_level != previous_level;
    snapshot.active = true;
    previous_level = requested_level;
    bool control_due = false;
    if (inhibited) {
        if (snapshot.phase != HEATER_INHIBITED || changed) {
            enter_phase(HEATER_INHIBITED);
        }
    } else if (resumed) {
        enter_phase(HEATER_PID);
        control_due = true;
    } else if (changed) {
        enter_phase(select_phase());
        control_due = true;
    } else if ((snapshot.phase == HEATER_PREHEAT &&
                tip_mdegc >= snapshot.target_mdegc - HEATER_PHASE_BAND_MDEGC) ||
               (snapshot.phase == HEATER_PRECOOL &&
                tip_mdegc <= snapshot.target_mdegc + HEATER_PHASE_BAND_MDEGC)) {
        enter_phase(HEATER_PID);
        control_due = true;
    }

    uint32_t limit_duty = maximum_duty(average_current_limit_ma);
    uint32_t limit_power = duty_power(limit_duty);
    if (control_due || (uint32_t)(now_ms - last_control_ms) >= HEATER_CONTROL_PERIOD_MS) {
        last_control_ms = now_ms;
        if (snapshot.phase == HEATER_PID) {
            requested_power_mw = PID_Update(&controller,
                snapshot.target_mdegc - tip_mdegc, HEATER_CONTROL_PERIOD_MS,
                0U, limit_power);
        }
    }

    uint32_t duty = 0U;
    if (snapshot.phase == HEATER_PREHEAT) {
        duty = limit_duty;
    } else if (snapshot.phase == HEATER_PID && battery_mv != 0U) {
        uint64_t compensated_duty = (uint64_t)requested_power_mw *
            HEATER_RESISTANCE_MOHM * HEATER_DUTY_SCALE /
            ((uint64_t)battery_mv * battery_mv);
        if (compensated_duty > limit_duty) {
            compensated_duty = limit_duty;
        }
        duty = (uint32_t)compensated_duty;
    }
    /* Reconcile voltage compensation and the live average current ceiling on
     * every tick, including those which do not run PID. Floor division ensures
     * the PWM cannot exceed the actual bound passed into conditional integration. */
    snapshot.power_mw = duty_power(duty);
    write_duty(duty);
}

Heater_Snapshot Heater_GetSnapshot(void)
{
    return snapshot;
}
