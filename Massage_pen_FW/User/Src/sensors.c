#include "sensors.h"
#include "thermistor_lut.h"

#include <stddef.h>
#include <string.h>

static ADC_HandleTypeDef *sensor_adc;
static const Sensors_AcquisitionOps *acquisition;
static Sensors_Snapshot snapshot;
static _Alignas(uint32_t) uint16_t dma_buffer[SENSORS_DMA_WORD_COUNT];
static volatile bool pending;
static volatile bool completed;
static volatile bool slot_failed;
static volatile uint8_t completed_channels;
static volatile uint16_t completed_battery;
static volatile uint16_t completed_tip;
static volatile unsigned expected_half;
static bool armed;
static volatile bool dma_ready;
static bool stopping;
static bool power_present;
static bool tick_seen;
static uint32_t last_tick;
static uint32_t settling_started;
static bool battery_initialized;
static bool tip_initialized;
static int64_t battery_filter;
static int64_t tip_filter;

static bool stop_acquisition(void)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    pending = false;
    completed = false;
    dma_ready = false;
    __set_PRIMASK(mask);
    if (armed || stopping)
    {
        stopping = true;
        if (!acquisition->stop_quiescent(sensor_adc))
        {
            return false;
        }
    }
    armed = false;
    stopping = false;
    expected_half = 0U;
    return true;
}

void Sensors_Init(ADC_HandleTypeDef *adc, const Sensors_AcquisitionOps *ops)
{
    /* Initialization occurs once before interrupts/acquisition are enabled. */
    sensor_adc = adc;
    acquisition = ops;
    memset(&snapshot, 0, sizeof(snapshot));
    pending = completed = slot_failed = dma_ready = armed = stopping = power_present = tick_seen = false;
    expected_half = 0U;
    battery_initialized = tip_initialized = false;
}

static void fail_channel(uint8_t *failures)
{
    if (*failures < SENSORS_FAILURE_LIMIT)
    {
        ++*failures;
    }
}

static int32_t interpolate_temperature(uint32_t ratio)
{
    /* Electrical validity is handled separately. Beyond the model range,
     * clamp conservatively; the hot endpoint still exceeds heater protections. */
    if (ratio <= thermistor_lut[0].divider_ratio_scaled)
    {
        return thermistor_lut[0].temperature_mdegc;
    }
    for (unsigned i = 1U; i < THERMISTOR_LUT_ENTRY_COUNT; ++i)
    {
        if (ratio <= thermistor_lut[i].divider_ratio_scaled)
        {
            const thermistor_lut_entry_t *low = &thermistor_lut[i - 1U];
            const thermistor_lut_entry_t *high = &thermistor_lut[i];
            return low->temperature_mdegc + (int32_t)
                (((int64_t)(ratio - low->divider_ratio_scaled) *
                  (high->temperature_mdegc - low->temperature_mdegc)) /
                 (high->divider_ratio_scaled - low->divider_ratio_scaled));
        }
    }
    return thermistor_lut[THERMISTOR_LUT_ENTRY_COUNT - 1U].temperature_mdegc;
}

static int64_t filter_sample(int64_t state, int32_t sample, bool initialized)
{
    int64_t target = (int64_t)sample * SENSORS_FILTER_FRACTION_SCALE;
    return initialized ? state + (target - state) / SENSORS_FILTER_DIVISOR : target;
}

static void consume_frame(uint16_t battery_sum, uint16_t tip_sum, uint8_t channels)
{
    bool battery_acquired = (channels & SENSORS_CHANNEL_BATTERY) != 0U;
    bool tip_acquired = (channels & SENSORS_CHANNEL_TIP) != 0U;
    uint32_t battery_count = battery_sum / SENSORS_ADC_OVERSAMPLING_RATIO;
    uint32_t tip_count = tip_sum / SENSORS_ADC_OVERSAMPLING_RATIO;
    uint32_t battery_mv = (uint32_t)((uint64_t)battery_count *
        SENSORS_ADC_REFERENCE_MV * BATTERY_SENSE_VOLTAGE_MULTIPLIER /
        SENSORS_ADC_MAX_COUNT);
    uint32_t tip_mv = (uint32_t)((uint64_t)tip_count *
        SENSORS_ADC_REFERENCE_MV / SENSORS_ADC_MAX_COUNT);

    if (battery_acquired)
    {
        ++snapshot.battery_sequence;
        snapshot.battery_failures = 0U;
        snapshot.battery_mv = battery_mv;
        snapshot.battery_valid = battery_mv <= SENSORS_BATTERY_MAX_MV;
        snapshot.invalid_battery |= !snapshot.battery_valid;
        if (snapshot.battery_valid && (battery_initialized ||
            (tip_acquired && battery_mv != 0U &&
             tip_mv > SENSORS_TIP_RAIL_MARGIN_MV &&
             tip_mv < SENSORS_ADC_REFERENCE_MV - SENSORS_TIP_RAIL_MARGIN_MV)))
        {
            battery_filter = filter_sample(battery_filter, (int32_t)battery_mv,
                                           battery_initialized);
            battery_initialized = true;
            snapshot.filtered_battery_mv = (uint32_t)
                (battery_filter / SENSORS_FILTER_FRACTION_SCALE);
        }
    }
    else
    {
        fail_channel(&snapshot.battery_failures);
    }

    if (!tip_acquired)
    {
        fail_channel(&snapshot.tip_failures);
    }
    else if (tip_mv <= SENSORS_TIP_RAIL_MARGIN_MV ||
             tip_mv >= SENSORS_ADC_REFERENCE_MV - SENSORS_TIP_RAIL_MARGIN_MV)
    {
        snapshot.tip_failures = 0U;
        snapshot.tip_valid = false;
        snapshot.invalid_tip = true;
    }
    else if (!battery_acquired)
    {
        /* Acquisition succeeded, but cannot convert against an older battery. */
        snapshot.tip_failures = 0U;
    }
    else if (battery_mv == 0U || !snapshot.battery_valid)
    {
        snapshot.tip_failures = 0U;
        snapshot.tip_valid = false;
    }
    else
    {
        snapshot.tip_failures = 0U;
        uint32_t ratio = (uint32_t)((uint64_t)tip_mv *
                                    THERMISTOR_LUT_RATIO_SCALE / battery_mv);
        snapshot.tip_mdegc = interpolate_temperature(ratio);
        snapshot.tip_valid = true;
        tip_filter = filter_sample(tip_filter, snapshot.tip_mdegc, tip_initialized);
        tip_initialized = true;
        snapshot.filtered_tip_mdegc = (int32_t)
            (tip_filter / SENSORS_FILTER_FRACTION_SCALE);
    }
    snapshot.ready = battery_initialized && tip_initialized &&
                     !snapshot.invalid_battery && !snapshot.invalid_tip;
    snapshot.acquisition_fault =
        snapshot.battery_failures >= SENSORS_FAILURE_LIMIT ||
        snapshot.tip_failures >= SENSORS_FAILURE_LIMIT;
}

static void miss_slots(uint32_t count)
{
    if (count > SENSORS_FAILURE_LIMIT) count = SENSORS_FAILURE_LIMIT;
    while (count-- != 0U) consume_frame(0U, 0U, 0U);
}

void Sensors_Update(bool power_available)
{
    uint32_t now = HAL_GetTick();
    bool adapter_available = sensor_adc != NULL && acquisition != NULL &&
        acquisition->arm != NULL && acquisition->trigger != NULL &&
        acquisition->stop_quiescent != NULL;
    if (!power_available || !adapter_available)
    {
        if (adapter_available) { (void)stop_acquisition(); }
        uint32_t sequence = snapshot.battery_sequence;
        memset(&snapshot, 0, sizeof(snapshot));
        snapshot.battery_sequence = sequence;
        power_present = false;
        tick_seen = false;
        battery_initialized = tip_initialized = false;
        return;
    }
    if (stopping)
    {
        if (!stop_acquisition())
        {
            /* Cancellation still owns the hardware, so no rearm is safe.
             * Every following powered acquisition slot is unavailable. */
            if (power_present && (!tick_seen || now != last_tick))
            {
                uint32_t slots = tick_seen ? (uint32_t)(now - last_tick) : 1U;
                tick_seen = true;
                last_tick = now;
                snapshot.fresh = false;
                snapshot.battery_fresh = snapshot.battery_current_valid = false;
                miss_slots(slots);
            }
            return;
        }
    }
    if (!power_present)
    {
        power_present = true;
        settling_started = now;
        tick_seen = false;
    }
    snapshot.available = true;
    if ((uint32_t)(now - settling_started) < SENSORS_POWER_SETTLE_MS ||
        (tick_seen && now == last_tick))
    {
        return;
    }
    uint32_t missed = tick_seen ? (uint32_t)(now - last_tick) - 1U : 0U;
    tick_seen = true;
    last_tick = now;
    snapshot.fresh = false;
    snapshot.battery_fresh = snapshot.battery_current_valid = false;

    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    bool was_pending = pending;
    bool was_completed = completed;
    uint8_t channels = completed_channels;
    uint16_t battery = completed_battery;
    uint16_t tip = completed_tip;
    pending = completed = slot_failed = false;
    __set_PRIMASK(mask);

    if (was_pending)
    {
        consume_frame(battery, tip, was_completed ? channels : 0U);
        snapshot.battery_fresh = was_completed &&
            (channels & SENSORS_CHANNEL_BATTERY) != 0U && missed == 0U;
        snapshot.battery_current_valid = snapshot.battery_fresh && snapshot.battery_valid;
        snapshot.fresh = was_completed && channels == SENSORS_CHANNEL_ALL && missed == 0U;
        if (!was_completed && !stop_acquisition()) { miss_slots(missed); return; }
    }
    miss_slots(missed);
    if (!armed)
    {
        /* Failed arm can leave peripheral activity; require quiescence too. */
        armed = true;
        if (!acquisition->arm(sensor_adc, dma_buffer, SENSORS_DMA_WORD_COUNT))
        {
            pending = true;
            return;
        }
        dma_ready = true;
    }
    /* Publish the slot before triggering: completion may interrupt trigger. */
    pending = true;
    if (!acquisition->trigger(sensor_adc))
    {
        slot_failed = true;
        completed = false;
    }
}

Sensors_Snapshot Sensors_GetSnapshot(void)
{
    return snapshot;
}

static void publish_half(ADC_HandleTypeDef *adc, unsigned half, uint8_t channels)
{
    if (adc != sensor_adc || !pending || completed || !dma_ready ||
        stopping || slot_failed || half != expected_half)
    {
        return;
    }
    completed_battery = dma_buffer[half * SENSORS_FRAME_WORD_COUNT +
                                   SENSORS_FRAME_BATTERY_INDEX];
    completed_tip = dma_buffer[half * SENSORS_FRAME_WORD_COUNT +
                               SENSORS_FRAME_TIP_INDEX];
    completed_channels = channels & SENSORS_CHANNEL_ALL;
    expected_half ^= 1U;
    completed = true;
}

void Sensors_OnDmaHalf(ADC_HandleTypeDef *adc, uint8_t acquired_channels)
{
    publish_half(adc, 0U, acquired_channels);
}

void Sensors_OnDmaFull(ADC_HandleTypeDef *adc, uint8_t acquired_channels)
{
    publish_half(adc, 1U, acquired_channels);
}

void Sensors_OnDmaError(ADC_HandleTypeDef *adc)
{
    /* No half phase can be assumed after a DMA error. Foreground treats this
     * as missing and must stop/drain before retrying. */
    if (adc == sensor_adc && pending)
    {
        slot_failed = true;
        completed = false;
    }
}

bool Sensors_IsQuiescent(void)
{
    return !armed && !stopping;
}
