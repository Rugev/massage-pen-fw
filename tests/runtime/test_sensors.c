#include <assert.h>
#include <stdio.h>
#include "fake_hal.h"
#include "sensors.h"

static void reset(void)
{
    FakeHAL_ResetSensors();
    Sensors_Init(&fake_adc, FakeHAL_SensorOps());
}

static void start(void)
{
    reset();
    Sensors_Update(true);
    FakeHAL_AdvanceTick(10U);
    Sensors_Update(true);
}

static Sensors_Snapshot pair(uint16_t battery, uint16_t tip, uint8_t mask)
{
    FakeHAL_CompletePair(battery * 16U, tip * 16U, mask);
    FakeHAL_AdvanceTick(1U);
    Sensors_Update(true);
    return Sensors_GetSnapshot();
}

/* Catches acquisition before power/settling and duplicate same-tick triggers. */
static void test_power_settling_and_cadence(void)
{
    reset();
    Sensors_Update(false);
    assert(FakeHAL_TriggerCount() == 0U);
    Sensors_Update(true);
    FakeHAL_AdvanceTick(9U);
    Sensors_Update(true);
    assert(FakeHAL_TriggerCount() == 0U);
    FakeHAL_AdvanceTick(1U);
    Sensors_Update(true);
    assert(FakeHAL_TriggerCount() == 1U);
    Sensors_Update(true);
    assert(FakeHAL_TriggerCount() == 1U);
    assert(!Sensors_GetSnapshot().ready);
}

/* Catches reading the other half, mixing samples or duplicate consumption. */
static void test_half_full_matching_and_duplicates(void)
{
    start();
    Sensors_Snapshot s = pair(3276U, 1912U, SENSORS_CHANNEL_ALL);
    assert(s.fresh && s.ready && s.battery_mv == 4000U);
    assert(s.tip_mdegc == 24995L);
    Sensors_OnDmaHalf(&fake_adc, SENSORS_CHANNEL_ALL); /* duplicate old half */
    s = pair(2457U, 1435U, SENSORS_CHANNEL_ALL);
    assert(s.fresh && s.battery_mv == 3000U && s.tip_mdegc == 25026L);
    Sensors_Update(true);
    assert(FakeHAL_TriggerCount() == 3U);
    s = pair(3276U, 1938U, SENSORS_CHANNEL_ALL);
    assert(s.battery_mv == 4000U && s.tip_mdegc == 25494L);
}

/* Catches early/duplicate miss counting, off-by-one limit and global reset. */
static void test_missing_and_channel_failure_limits(void)
{
    start();
    for (unsigned i = 1U; i <= 10U; ++i) {
        FakeHAL_AdvanceTick(1U);
        Sensors_Update(true);
        Sensors_Snapshot s = Sensors_GetSnapshot();
        assert(s.battery_failures == i && s.tip_failures == i);
        assert(s.acquisition_fault == (i == 10U));
        Sensors_Update(true);
        assert(Sensors_GetSnapshot().battery_failures == i);
    }
    Sensors_Snapshot s = pair(3276U, 1912U, SENSORS_CHANNEL_BATTERY);
    assert(s.battery_failures == 0U && s.tip_failures == 10U);
    s = pair(3276U, 1912U, SENSORS_CHANNEL_TIP);
    assert(s.battery_failures == 1U && s.tip_failures == 0U);
}

/* Catches restart without a stopped/drained ADC, old epochs and stale readiness. */
static void test_power_stop_and_late_callbacks(void)
{
    start();
    assert(pair(3276U, 1912U, SENSORS_CHANNEL_ALL).ready);
    FakeHAL_QueuePair(4095U * 16U, 4000U * 16U);
    FakeHAL_SetStopReady(false);
    Sensors_Update(false);
    assert(!Sensors_GetSnapshot().ready && !Sensors_GetSnapshot().available);
    Sensors_OnDmaFull(&fake_adc, SENSORS_CHANNEL_ALL);
    FakeHAL_AdvanceTick(20U);
    Sensors_Update(true);
    assert(FakeHAL_TriggerCount() == 2U);
    FakeHAL_SetStopReady(true);
    FakeHAL_AdvanceTick(1U);
    Sensors_Update(true);
    FakeHAL_AdvanceTick(10U);
    Sensors_Update(true);
    FakeHAL_DeliverQueuedPair(); /* stop drained the old pending interrupt */
    assert(!Sensors_GetSnapshot().ready);
    Sensors_Snapshot s = pair(2457U, 1435U, SENSORS_CHANNEL_ALL);
    assert(s.ready && s.battery_mv == 3000U);
    assert(s.filtered_battery_mv == 3000U && s.filtered_tip_mdegc == s.tip_mdegc);
}

/* Catches missing normalization/interpolation and control data used as protection. */
static void test_conversion_filter_and_protection(void)
{
    start();
    Sensors_Snapshot s = pair(3276U, 1912U, SENSORS_CHANNEL_ALL);
    assert(s.filtered_battery_mv == 4000U && s.filtered_tip_mdegc == 24995L);
    s = pair(2457U, 1435U, SENSORS_CHANNEL_ALL);
    assert(s.filtered_battery_mv == 3937U);
    assert(s.filtered_tip_mdegc == 24996L);
    s = pair(3276U, 3280U, SENSORS_CHANNEL_ALL);
    assert(s.tip_mdegc > 49500L && s.filtered_tip_mdegc < 30000L);
    for (unsigned i = 0U; i < 100U; ++i) {
        s = pair(3276U, 1912U, SENSORS_CHANNEL_ALL);
    }
    assert(s.filtered_battery_mv >= 3999U && s.filtered_battery_mv < 4000U);
    for (unsigned i = 0U; i < 200U; ++i) {
        s = pair(2457U, 1435U, SENSORS_CHANNEL_ALL);
    }
    assert(s.filtered_battery_mv == 3000U);
}

/* Catches treating valid undervoltage as faulty, rail/high-voltage boundaries. */
static void test_electrical_validity(void)
{
    start();
    Sensors_Snapshot s = pair(1556U, 908U, SENSORS_CHANNEL_ALL);
    assert(s.battery_valid && !s.invalid_battery && s.battery_mv == 1899U);
    assert(s.tip_valid);
    s = pair(3687U, 1912U, SENSORS_CHANNEL_ALL); /* 4501 mV */
    assert(s.invalid_battery && !s.battery_valid && s.battery_mv == 4501U);
    start();
    s = pair(3276U, 34U, SENSORS_CHANNEL_ALL); /* floor 20 mV */
    assert(s.invalid_tip && !s.tip_valid);
    start();
    s = pair(3276U, 4063U, SENSORS_CHANNEL_ALL); /* 2480 mV */
    assert(s.invalid_tip && !s.tip_valid);
}

/* Catches silently substituting polling/free-running production acquisition. */
static void test_missing_adapter_is_gated(void)
{
    FakeHAL_ResetSensors();
    Sensors_Init(&fake_adc, NULL);
    Sensors_Update(true);
    FakeHAL_AdvanceTick(100U);
    Sensors_Update(true);
    assert(!Sensors_GetSnapshot().available && !Sensors_GetSnapshot().ready);
    assert(FakeHAL_TriggerCount() == 0U);
}

/* Catches accepting completion after error and phase mismatch after cancellation. */
static void test_dma_error_invalidates_the_slot(void)
{
    start();
    Sensors_OnDmaError(&fake_adc);
    FakeHAL_CompletePair(3276U * 16U, 1912U * 16U, SENSORS_CHANNEL_ALL);
    FakeHAL_AdvanceTick(1U);
    Sensors_Update(true);
    Sensors_Snapshot s = Sensors_GetSnapshot();
    assert(!s.ready && !s.fresh && s.battery_failures == 1U && s.tip_failures == 1U);
    s = pair(2457U, 1435U, SENSORS_CHANNEL_ALL);
    assert(s.ready && s.battery_mv == 3000U);
}

/* Catches discarding usable control data during the nine allowed retries. */
static void test_retries_retain_last_valid_and_trigger_failure_counts_once(void)
{
    start();
    assert(pair(3276U, 1912U, SENSORS_CHANNEL_ALL).ready);
    for (unsigned i = 1U; i <= 9U; ++i) {
        FakeHAL_AdvanceTick(1U);
        Sensors_Update(true);
        Sensors_Snapshot s = Sensors_GetSnapshot();
        assert(s.ready && !s.acquisition_fault && s.battery_failures == i);
        assert(s.battery_mv == 4000U && s.tip_mdegc == 24995L);
    }
    assert(pair(2457U, 1435U, SENSORS_CHANNEL_ALL).battery_failures == 0U);
    /* Fail trigger for the slot after consuming the successful pair. */
    FakeHAL_SetTriggerReady(false);
    (void)pair(3276U, 1912U, SENSORS_CHANNEL_ALL);
    assert(Sensors_GetSnapshot().battery_failures == 0U);
    FakeHAL_AdvanceTick(1U);
    Sensors_Update(true);
    assert(Sensors_GetSnapshot().battery_failures == 1U);
    Sensors_Update(true);
    assert(Sensors_GetSnapshot().battery_failures == 1U);
    FakeHAL_SetTriggerReady(true);
    FakeHAL_AdvanceTick(1U);
    Sensors_Update(true);
    assert(Sensors_GetSnapshot().battery_failures == 2U);
    assert(pair(3276U, 1912U, SENSORS_CHANNEL_ALL).battery_failures == 0U);
}

/* Catches a wraparound delay and publishing an unmatched tip with old battery. */
static void test_rollover_and_partial_frame(void)
{
    reset();
    FakeHAL_SetTick(UINT32_MAX - 5U);
    Sensors_Update(true);
    FakeHAL_AdvanceTick(10U);
    Sensors_Update(true);
    assert(FakeHAL_TriggerCount() == 1U);
    Sensors_Snapshot s = pair(3276U, 1912U, SENSORS_CHANNEL_ALL);
    s = pair(2457U, 3280U, SENSORS_CHANNEL_TIP);
    assert(!s.fresh && s.tip_mdegc == 24995L);
    assert(s.tip_failures == 0U && s.battery_failures == 1U);
    /* Raw electrical invalidity is knowable even without battery. */
    s = pair(2457U, 34U, SENSORS_CHANNEL_TIP);
    assert(s.invalid_tip);
}

/* Catches treating failed DMA setup as a valid frame or a physical sensor fault. */
static void test_failed_arm_and_zero_battery(void)
{
    reset();
    FakeHAL_SetArmReady(false);
    Sensors_Update(true);
    FakeHAL_AdvanceTick(10U);
    Sensors_Update(true);
    Sensors_OnDmaHalf(&fake_adc, SENSORS_CHANNEL_ALL);
    FakeHAL_AdvanceTick(1U);
    Sensors_Update(true);
    Sensors_Snapshot s = Sensors_GetSnapshot();
    assert(!s.invalid_tip && !s.invalid_battery && !s.ready);
    assert(s.battery_failures == 1U && s.tip_failures == 1U);
    FakeHAL_SetArmReady(true);
    FakeHAL_AdvanceTick(1U);
    Sensors_Update(true);
    s = pair(0U, 1000U, SENSORS_CHANNEL_ALL);
    assert(s.battery_valid && !s.invalid_battery && s.battery_mv == 0U);
    assert(!s.tip_valid && !s.invalid_tip && !s.ready);
}

/* Catches initializing a control filter from an unmatched startup sample. */
static void test_initial_filters_require_a_valid_pair(void)
{
    start();
    Sensors_Snapshot s = pair(2457U, 1435U, SENSORS_CHANNEL_BATTERY);
    assert(!s.ready);
    s = pair(3276U, 1912U, SENSORS_CHANNEL_ALL);
    assert(s.ready && s.filtered_battery_mv == 4000U);
    assert(s.filtered_tip_mdegc == 24995L);
}

/* Catches indefinite healthy readiness when ADC cancellation cannot finish. */
static void test_blocked_stop_exhausts_failures_once_per_tick(void)
{
    start();
    assert(pair(3276U, 1912U, SENSORS_CHANNEL_ALL).ready);
    FakeHAL_SetStopReady(false);
    for (unsigned i = 1U; i <= 12U; ++i) {
        FakeHAL_AdvanceTick(1U);
        Sensors_Update(true);
        Sensors_Snapshot s = Sensors_GetSnapshot();
        unsigned expected_failures = i < 10U ? i : 10U;
        assert(s.battery_failures == expected_failures);
        assert(s.tip_failures == expected_failures);
        assert(s.acquisition_fault == (i >= 10U));
        assert(!s.fresh && s.battery_mv == 4000U && s.tip_mdegc == 24995L);
        assert(FakeHAL_TriggerCount() == 2U); /* Rearm remains prohibited. */
        Sensors_Update(true);
        assert(Sensors_GetSnapshot().battery_failures == expected_failures);
        assert(Sensors_GetSnapshot().tip_failures == expected_failures);
    }
    FakeHAL_SetStopReady(true);
    FakeHAL_AdvanceTick(1U);
    Sensors_Update(true);
    assert(FakeHAL_TriggerCount() == 3U);
    Sensors_Snapshot s = pair(3276U, 1912U, SENSORS_CHANNEL_ALL);
    assert(s.fresh && s.battery_failures == 0U && s.tip_failures == 0U);
}

static void test_quiescence_observes_owned_dma_and_failed_stop(void)
{
    FakeHAL_ResetSensors();
    Sensors_Init(&fake_adc, NULL);
    assert(Sensors_IsQuiescent());
    reset();
    assert(Sensors_IsQuiescent());
    Sensors_Update(true);
    FakeHAL_AdvanceTick(SENSORS_POWER_SETTLE_MS);
    Sensors_Update(true);
    assert(!Sensors_IsQuiescent());
    FakeHAL_SetStopReady(false);
    Sensors_Update(false);
    assert(!Sensors_IsQuiescent());
    assert(!Sensors_GetSnapshot().available); /* Unavailable does not mean drained. */
    FakeHAL_AdvanceTick(1U);
    Sensors_Update(false);
    assert(!Sensors_IsQuiescent());
    FakeHAL_SetStopReady(true);
    Sensors_Update(false);
    assert(Sensors_IsQuiescent());
}

/* A completed old pair cannot hide unscheduled acquisition slots. */
static void test_foreground_gap_counts_slots_without_catchup(void)
{
    start();
    FakeHAL_CompletePair(3276U * 16U, 1912U * 16U, SENSORS_CHANNEL_ALL);
    FakeHAL_AdvanceTick(5U);
    Sensors_Update(true);
    Sensors_Snapshot s = Sensors_GetSnapshot();
    assert(!s.fresh && s.battery_failures == 4U && s.tip_failures == 4U);
    assert(FakeHAL_TriggerCount() == 2U);
    Sensors_Update(true);
    assert(FakeHAL_TriggerCount() == 2U);
    FakeHAL_AdvanceTick(6U);
    Sensors_Update(true);
    assert(Sensors_GetSnapshot().acquisition_fault);
    assert(FakeHAL_TriggerCount() == 3U);
    assert(pair(3276U, 1912U, SENSORS_CHANNEL_ALL).fresh);
}

static void test_independent_current_battery(void)
{
    start();
    assert(!Sensors_GetSnapshot().battery_fresh);
    Sensors_Snapshot s = pair(2047U, 1912U, SENSORS_CHANNEL_BATTERY);
    assert(s.battery_fresh && s.battery_current_valid && !s.fresh && !s.ready);
    assert(s.battery_mv == 2499U && s.battery_sequence == 1U);
    s = pair(3276U, 1912U, SENSORS_CHANNEL_ALL);
    assert(s.ready && s.battery_sequence == 2U);
    s = pair(3276U, 1912U, SENSORS_CHANNEL_TIP);
    assert(s.ready && !s.battery_fresh && !s.battery_current_valid);
    assert(s.battery_sequence == 2U && s.battery_mv == 4000U);
    FakeHAL_CompletePair(2047U*16U, 1912U*16U, SENSORS_CHANNEL_BATTERY);
    FakeHAL_AdvanceTick(5U); Sensors_Update(true);
    s = Sensors_GetSnapshot();
    assert(!s.battery_fresh && !s.battery_current_valid && s.battery_sequence == 3U);
    s = pair(3687U, 1912U, SENSORS_CHANNEL_BATTERY);
    assert(s.battery_fresh && !s.battery_current_valid && s.invalid_battery);
    Sensors_Update(false);
    Sensors_OnDmaHalf(&fake_adc, SENSORS_CHANNEL_ALL);
    s = Sensors_GetSnapshot();
    assert(!s.battery_fresh && !s.battery_current_valid && s.battery_sequence == 4U);
    Sensors_Update(true); FakeHAL_AdvanceTick(9U); Sensors_Update(true);
    assert(!Sensors_GetSnapshot().battery_fresh);
    FakeHAL_AdvanceTick(1U); Sensors_Update(true);
    s = pair(3276U, 1912U, SENSORS_CHANNEL_BATTERY);
    assert(s.battery_fresh && s.battery_current_valid && s.battery_sequence == 5U);
}

int main(void)
{
    test_independent_current_battery();
    test_foreground_gap_counts_slots_without_catchup();
    test_quiescence_observes_owned_dma_and_failed_stop();
    test_blocked_stop_exhausts_failures_once_per_tick();
    test_initial_filters_require_a_valid_pair();
    test_failed_arm_and_zero_battery();
    test_dma_error_invalidates_the_slot();
    test_retries_retain_last_valid_and_trigger_failure_counts_once();
    test_rollover_and_partial_frame();
    test_power_settling_and_cadence();
    test_half_full_matching_and_duplicates();
    test_missing_and_channel_failure_limits();
    test_power_stop_and_late_callbacks();
    test_conversion_filter_and_protection();
    test_electrical_validity();
    test_missing_adapter_is_gated();
    puts("sensor tests passed");
}
