#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "heater.h"
#include "pid.h"

static uint32_t electrical_duty;
static unsigned pwm_writes;
static int pwm_context;

static void set_duty(void *context, uint32_t duty_ppm)
{
    assert(context == &pwm_context);
    assert(duty_ppm <= 1000000U);
    electrical_duty = duty_ppm;
    ++pwm_writes;
}

static const Heater_PwmOps pwm = { .set_duty = set_duty };

static Sensors_Snapshot measurement(uint32_t mv, int32_t temperature)
{
    Sensors_Snapshot s = {0};
    s.available = s.ready = s.fresh = s.battery_valid = s.tip_valid = true;
    s.battery_mv = s.filtered_battery_mv = mv;
    s.tip_mdegc = s.filtered_tip_mdegc = temperature;
    return s;
}

static void reset(void)
{
    electrical_duty = 123456U;
    pwm_writes = 0U;
    Heater_Init(&pwm, &pwm_context);
    assert(electrical_duty == 0U && pwm_writes == 1U);
}

static Heater_Snapshot update(uint32_t now, uint8_t level, bool authorized,
                              uint32_t limit_ma, Sensors_Snapshot *s)
{
    Heater_Update(now, level, authorized, limit_ma, s);
    Heater_Snapshot state = Heater_GetSnapshot();
    assert(state.duty_ppm == electrical_duty);
    return state;
}

/* Catches energizing from loaded settings, absent hardware or missing startup data. */
static void test_readiness_and_disable(void)
{
    Sensors_Snapshot s = measurement(4000U, 25000L);
    reset();
    assert(update(0U, 1U, false, 2000U, &s).duty_ppm == 0U);
    s.ready = false;
    assert(update(1U, 1U, true, 2000U, &s).duty_ppm == 0U);
    s.ready = true;
    assert(update(2U, 1U, true, 2000U, &s).phase == HEATER_PREHEAT);
    assert(update(3U, 0U, true, 2000U, &s).phase == HEATER_OFF);
    assert(electrical_duty == 0U);
    assert(update(4U, 1U, true, 2000U, &s).duty_ppm > 0U);
    assert(update(5U, 1U, false, 2000U, &s).duty_ppm == 0U);
    assert(update(6U, 99U, true, 2000U, &s).phase == HEATER_OFF);
    Heater_Init(NULL, NULL);
    Heater_Update(7U, 1U, true, 2000U, &s);
    assert(!Heater_GetSnapshot().available && Heater_GetSnapshot().duty_ppm == 0U);
    Heater_PwmOps incomplete = {0};
    Heater_Init(&incomplete, &pwm_context);
    assert(!Heater_GetSnapshot().available);
}

/* Catches confusing instantaneous with average current, overflow and stale limits. */
static void test_voltage_and_current_limits(void)
{
    Sensors_Snapshot s = measurement(4000U, 25000L);
    reset();
    Heater_Snapshot h = update(0U, 1U, true, 2000U, &s);
    assert(h.power_mw == 8000U && h.duty_ppm == 1000000U);
    h = update(1U, 1U, true, 1000U, &s);
    assert(h.power_mw == 4000U && h.duty_ppm == 500000U);
    s = measurement(4500U, -20000L);
    h = update(2U, 1U, true, 2000U, &s);
    assert(h.duty_ppm <= 888888U); /* Average <= 2 A at 4.5 V, R = 2 ohm. */
    h = update(50U, 1U, true, 2000U, &s);
    assert(h.power_mw == 8999U && h.duty_ppm == 888888U);
    s = measurement(3000U, 25000L);
    h = update(51U, 1U, true, 2000U, &s);
    assert(h.power_mw == 4500U && h.duty_ppm == 1000000U);
    h = update(52U, 1U, true, 1000U, &s);
    assert(h.power_mw == 2999U && h.duty_ppm == 666666U);
    assert(update(53U, 1U, true, 0U, &s).duty_ppm == 0U);
    s = measurement(0U, 25000L);
    assert(update(54U, 1U, true, 2000U, &s).duty_ppm == 0U);
    s = measurement(1U, -20000L);
    assert(update(100U, 3U, true, UINT32_MAX, &s).duty_ppm <= 1000000U);
}

/* Catches changing power instead of duty when battery voltage changes in PID. */
static void test_compensated_pid_power_and_cadence(void)
{
    Sensors_Snapshot s = measurement(4000U, 39000L);
    reset();
    Heater_Snapshot h = update(100U, 1U, true, 2000U, &s);
    assert(h.phase == HEATER_PID && h.power_mw == 501U && h.duty_ppm == 62625U);
    h = update(149U, 1U, true, 2000U, &s);
    assert(h.power_mw == 501U);
    s = measurement(3000U, 39000L);
    h = update(149U, 1U, true, 2000U, &s);
    /* 4500 mW full power * 111333 / 1000000 = 500.9985 mW. */
    assert(h.power_mw == 500U && h.duty_ppm == 111333U);
    h = update(150U, 1U, true, 2000U, &s);
    assert(h.power_mw == 501U && h.duty_ppm == 111555U);
    reset();
    h = update(UINT32_MAX - 20U, 1U, true, 2000U, &s);
    assert(h.power_mw == 500U);
    assert(update(29U, 1U, true, 2000U, &s).power_mw == 501U);
}

/* Catches narrowing compensated duty before clamping at low valid battery. */
static void test_low_voltage_compensation_uses_wide_duty(void)
{
    Sensors_Snapshot s = measurement(4500U, 38000L);
    reset();
    for (unsigned i = 0U; i < 1655U; ++i) {
        (void)update(i * 50U, 1U, true, 2000U, &s);
    }
    /* P = 1000 mW, I = 1655 * 2 mW: retained demand is 4310 mW. */
    s = measurement(1U, 38000L);
    Heater_Snapshot h = update(82701U, 1U, true, 2000U, &s);
    assert(h.duty_ppm == 1000000U && !h.fault_temperature);
    s = measurement(0U, 38000L);
    assert(update(82702U, 1U, true, 2000U, &s).duty_ppm == 0U);
}

/* Catches waiting for PID cadence to enforce a changed current ceiling. */
static void test_pid_live_limit_clamps_before_control_tick(void)
{
    Sensors_Snapshot s = measurement(4000U, 38000L);
    reset();
    for (unsigned i = 0U; i < 2000U; ++i) {
        (void)update(i * 50U, 1U, true, 2000U, &s);
    }
    assert(Heater_GetSnapshot().power_mw == 5000U);
    Heater_Snapshot h = update(99951U, 1U, true, 1000U, &s);
    assert(h.duty_ppm == 500000U && h.power_mw == 4000U);
    s = measurement(4500U, 38000L);
    h = update(99952U, 1U, true, 1000U, &s);
    assert(h.duty_ppm == 444444U && h.power_mw == 4499U);
}

/* Catches wrong inclusive boundaries or reselecting phases after disturbances. */
static void test_phase_selection_and_completion(void)
{
    const int32_t temperatures[] = {37999L, 38000L, 42000L, 42001L};
    const Heater_Phase phases[] = {HEATER_PREHEAT, HEATER_PID, HEATER_PID, HEATER_PRECOOL};
    for (unsigned i = 0U; i < 4U; ++i) {
        Sensors_Snapshot s = measurement(4000U, temperatures[i]);
        reset();
        assert(update(0U, 1U, true, 2000U, &s).phase == phases[i]);
    }
    Sensors_Snapshot s = measurement(4000U, 25000L);
    reset();
    assert(update(0U, 1U, true, 2000U, &s).phase == HEATER_PREHEAT);
    s = measurement(4000U, 38000L);
    assert(update(1U, 1U, true, 2000U, &s).phase == HEATER_PID);
    s = measurement(4000U, 25000L);
    assert(update(50U, 1U, true, 2000U, &s).phase == HEATER_PID);
    s = measurement(4000U, 45000L);
    assert(update(51U, 1U, true, 2000U, &s).phase == HEATER_PID);
    assert(update(52U, 2U, true, 2000U, &s).phase == HEATER_PID);
    s = measurement(4000U, 47000L);
    assert(update(53U, 1U, true, 2000U, &s).phase == HEATER_PRECOOL);
    s = measurement(4000U, 42000L);
    assert(update(54U, 1U, true, 2000U, &s).phase == HEATER_PID);
    s = measurement(4000U, 47000L);
    assert(update(100U, 1U, true, 2000U, &s).phase == HEATER_PID);
}

/* Catches using filtered/cadenced protection or resuming at target equality. */
static void test_raw_protection_and_latching(void)
{
    Sensors_Snapshot s = measurement(4000U, 39000L);
    reset();
    assert(update(0U, 1U, true, 2000U, &s).power_mw == 501U);
    s.tip_mdegc = 48999L;
    assert(update(1U, 1U, true, 2000U, &s).phase == HEATER_PID);
    s.tip_mdegc = 49000L;
    assert(update(1U, 1U, true, 2000U, &s).phase == HEATER_INHIBITED);
    assert(electrical_duty == 0U);
    s.tip_mdegc = 40000L;
    assert(update(2U, 1U, true, 2000U, &s).phase == HEATER_INHIBITED);
    s.tip_mdegc = 39999L;
    assert(update(3U, 1U, true, 2000U, &s).phase == HEATER_PID);
    assert(Heater_GetSnapshot().power_mw == 501U); /* Integral was reset. */
    s.tip_mdegc = 49499L;
    assert(!update(4U, 1U, true, 2000U, &s).fault_temperature);
    s.tip_mdegc = 49500L;
    Heater_Snapshot h = update(4U, 1U, true, 2000U, &s);
    assert(h.fault_temperature && h.duty_ppm == 0U);
    s = measurement(4000U, 25000L);
    assert(update(5U, 0U, false, 2000U, &s).fault_temperature);
    assert(update(6U, 1U, true, 2000U, &s).duty_ppm == 0U);
    reset();
    s = measurement(4000U, 49500L);
    s.fresh = false;
    assert(!update(0U, 0U, false, 2000U, &s).fault_temperature);
    s.fresh = true;
    assert(update(1U, 0U, false, 2000U, &s).fault_temperature);
}

/* Catches dropping retained controls during retries or ignoring physical invalidity. */
static void test_acquisition_retries_and_fault_gates(void)
{
    Sensors_Snapshot s = measurement(4000U, 25000L);
    reset();
    assert(update(0U, 1U, true, 2000U, &s).duty_ppm == 1000000U);
    s.fresh = s.battery_valid = s.tip_valid = false;
    s.filtered_battery_mv = 0U;
    s.filtered_tip_mdegc = 80000L;
    for (unsigned i = 1U; i <= 9U; ++i) {
        assert(update(i * 50U, 1U, true, 2000U, &s).duty_ppm == 1000000U);
    }
    s.acquisition_fault = true;
    assert(update(451U, 1U, true, 2000U, &s).duty_ppm == 0U);
    reset();
    s = measurement(4000U, 25000L);
    assert(update(0U, 1U, true, 2000U, &s).duty_ppm > 0U);
    s.invalid_tip = true;
    assert(update(1U, 1U, true, 2000U, &s).duty_ppm == 0U);
    reset();
    s.invalid_tip = false;
    s.invalid_battery = true;
    assert(update(0U, 1U, true, 2000U, &s).duty_ppm == 0U);
}

/* Catches truncating sub-mW integral each tick, incorrect gains or negative feedback. */
static void test_pid_fractional_integral_and_negative_feedback(void)
{
    PID_Controller pid;
    PID_Reset(&pid);
    assert(PID_Update(&pid, 1000L, 50U, 0U, 8000U) == 501U);
    assert(PID_Update(&pid, 1000L, 50U, 0U, 8000U) == 502U);
    PID_Reset(&pid);
    for (unsigned i = 0U; i < 9U; ++i) {
        assert(PID_Update(&pid, 100L, 50U, 0U, 8000U) == 50U);
    }
    assert(PID_Update(&pid, 100L, 50U, 0U, 8000U) == 51U);
    PID_Reset(&pid);
    for (unsigned i = 0U; i < 1000U; ++i) {
        (void)PID_Update(&pid, 2000L, 50U, 0U, 8000U);
    }
    assert(PID_Update(&pid, -1000L, 50U, 0U, 8000U) == 999U);
}

/* Catches accumulating at actual saturation and refusing integration out of it. */
static void test_pid_antiwindup_and_recovery(void)
{
    PID_Controller pid;
    PID_Reset(&pid);
    for (unsigned i = 0U; i < 1000U; ++i) {
        assert(PID_Update(&pid, 20000L, 50U, 0U, 4000U) == 4000U);
    }
    assert(PID_Update(&pid, 0L, 50U, 0U, 4000U) == 0U);
    for (unsigned i = 0U; i < 1000U; ++i) {
        (void)PID_Update(&pid, 2000L, 50U, 0U, 8000U);
    }
    assert(PID_Update(&pid, 0L, 50U, 0U, 1000U) == 1000U);
    for (unsigned i = 0U; i < 1000U; ++i) {
        (void)PID_Update(&pid, -1000L, 50U, 0U, 1000U);
    }
    assert(PID_Update(&pid, 0L, 50U, 0U, 1000U) == 1000U);
    assert(PID_Update(&pid, -1000L, 50U, 0U, 1000U) == 0U);
    PID_Reset(&pid);
    assert(PID_Update(&pid, 68000L, 50U, 0U, 10125U) == 10125U);
    assert(PID_Update(&pid, -40000L, 50U, 0U, 10125U) == 0U);
    PID_Reset(&pid);
    assert(PID_Update(&pid, 1000L, 0U, 0U, 8000U) == 500U);
    assert(PID_Update(&pid, 1000L, 50U, 0U, 8000U) == 501U);
    assert(PID_Update(&pid, 1000L, 50U, 2000U, 1000U) == 1000U);
}

int main(void)
{
    test_readiness_and_disable();
    test_voltage_and_current_limits();
    test_compensated_pid_power_and_cadence();
    test_low_voltage_compensation_uses_wide_duty();
    test_pid_live_limit_clamps_before_control_tick();
    test_phase_selection_and_completion();
    test_raw_protection_and_latching();
    test_acquisition_retries_and_fault_gates();
    test_pid_fractional_integral_and_negative_feedback();
    test_pid_antiwindup_and_recovery();
    puts("heater/PID tests passed");
}
