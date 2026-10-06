#include "fake_hal.h"
#include "sensors.h"
#include <assert.h>
#include <stddef.h>

ADC_HandleTypeDef fake_adc;
static uint16_t *dma_buffer;
static bool dma_armed, conversion_pending, stop_ready, trigger_ready, arm_ready;
static uint32_t trigger_count, stop_count;
static unsigned dma_half;
static bool queued_pair;
static uint16_t queued_battery, queued_tip;

void FakeHAL_ResetSensors(void)
{
    FakeHAL_Reset();
    dma_buffer = NULL;
    dma_armed = conversion_pending = queued_pair = false;
    stop_ready = trigger_ready = arm_ready = true;
    trigger_count = stop_count = 0U;
    dma_half = 0U;
}

static bool fake_arm(ADC_HandleTypeDef *adc, uint16_t *buffer, uint32_t words)
{
    assert(adc == &fake_adc && words == 4U);
    assert(!dma_armed && !conversion_pending);
    if (!arm_ready) { return false; }
    dma_buffer = buffer;
    dma_armed = true;
    dma_half = 0U;
    return true;
}

static bool fake_trigger(ADC_HandleTypeDef *adc)
{
    assert(adc == &fake_adc && dma_armed && !conversion_pending);
    ++trigger_count;
    if (!trigger_ready) { return false; }
    conversion_pending = true;
    return true;
}

static bool fake_stop(ADC_HandleTypeDef *adc)
{
    assert(adc == &fake_adc);
    ++stop_count;
    if (!stop_ready) { return false; }
    dma_armed = conversion_pending = queued_pair = false;
    dma_buffer = NULL;
    return true;
}

const struct Sensors_AcquisitionOps *FakeHAL_SensorOps(void)
{
    static const Sensors_AcquisitionOps ops = {fake_arm, fake_trigger, fake_stop};
    return &ops;
}

void FakeHAL_CompletePair(uint16_t battery_sum, uint16_t tip_sum,
                          uint8_t acquired_channels)
{
    assert(dma_armed && conversion_pending);
    conversion_pending = false;
    dma_buffer[dma_half * 2U] = battery_sum;
    dma_buffer[dma_half * 2U + 1U] = tip_sum;
    if (dma_half == 0U) { Sensors_OnDmaHalf(&fake_adc, acquired_channels); }
    else { Sensors_OnDmaFull(&fake_adc, acquired_channels); }
    dma_half ^= 1U;
}

void FakeHAL_QueuePair(uint16_t battery_sum, uint16_t tip_sum)
{
    queued_pair = true;
    queued_battery = battery_sum;
    queued_tip = tip_sum;
}

void FakeHAL_DeliverQueuedPair(void)
{
    if (queued_pair) {
        queued_pair = false;
        FakeHAL_CompletePair(queued_battery, queued_tip, SENSORS_CHANNEL_ALL);
    }
}
void FakeHAL_SetStopReady(bool ready) { stop_ready = ready; }
void FakeHAL_SetArmReady(bool ready) { arm_ready = ready; }
void FakeHAL_SetTriggerReady(bool ready) { trigger_ready = ready; }
uint32_t FakeHAL_TriggerCount(void) { return trigger_count; }
uint32_t FakeHAL_StopCount(void) { return stop_count; }
