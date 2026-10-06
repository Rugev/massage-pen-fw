/* Battery voltage and heater temperature inputs. */
#ifndef USER_SENSORS_H
#define USER_SENSORS_H

/* Thermistor is the upper divider leg; fixed resistor is the lower leg. */
#define THERMISTOR_NOMINAL_RESISTANCE_OHM       10000U
#define THERMISTOR_NOMINAL_TEMP_MDEGC           25000L
#define THERMISTOR_BETA_K                       3435U
#define THERMISTOR_DIVIDER_BOTTOM_OHM           4120U

/* ADC reference is fixed; the thermistor divider supply is measured battery voltage. */
#define SENSORS_ADC_REFERENCE_MV                2500U
#define BATTERY_SENSE_VOLTAGE_MULTIPLIER        2U

/* Generator settings: divider ratio = input voltage / battery voltage. */
#define THERMISTOR_LUT_RATIO_SCALE              1000000U
#define THERMISTOR_LUT_MIN_TEMP_MDEGC           (-20000L)
#define THERMISTOR_LUT_MAX_TEMP_MDEGC           80000L
#define THERMISTOR_LUT_STEP_MDEGC               1000L

#include <stdbool.h>
#include <stdint.h>
#include "main.h"

/* Match generated 12-bit, 16x oversampling with no right shift. */
#define SENSORS_ADC_MAX_COUNT                    4095U
#define SENSORS_ADC_OVERSAMPLING_RATIO           16U
#define SENSORS_POWER_SETTLE_MS                  10U
#define SENSORS_ACQUISITION_PERIOD_MS            1U
#define SENSORS_FAILURE_LIMIT                   10U
#define SENSORS_TIP_RAIL_MARGIN_MV               20U
#define SENSORS_BATTERY_MAX_MV                   4500U
#define SENSORS_FILTER_DIVISOR                  16L
#define SENSORS_FILTER_FRACTION_SCALE            65536L
#define SENSORS_CHANNEL_BATTERY                  1U
#define SENSORS_CHANNEL_TIP                      2U
#define SENSORS_CHANNEL_ALL                      3U
#define SENSORS_DMA_WORD_COUNT                   4U
#define SENSORS_FRAME_WORD_COUNT                 2U
#define SENSORS_FRAME_BATTERY_INDEX              0U
#define SENSORS_FRAME_TIP_INDEX                  1U

/* arm prepares circular halfword DMA without starting conversion. Each trigger
 * starts exactly one battery/tip sequence; callbacks precede any reuse of that
 * half. stop_quiescent stops ADC/DMA and drains pending interrupts/callbacks;
 * false prevents rearm until a later successful stop. These contracts need
 * CubeMX two-channel circular DMA configuration; NULL keeps acquisition gated. */
typedef struct Sensors_AcquisitionOps {
    bool (*arm)(ADC_HandleTypeDef *adc, uint16_t *buffer, uint32_t word_count);
    bool (*trigger)(ADC_HandleTypeDef *adc);
    bool (*stop_quiescent)(ADC_HandleTypeDef *adc);
} Sensors_AcquisitionOps;

typedef struct {
    bool battery_fresh; /* Timely battery completion this tick, independent of tip. */
    bool battery_current_valid; /* Fresh battery passes electrical validity. */
    uint32_t battery_sequence; /* Accepted battery acquisitions; retained across power loss. */
    bool available; /* Complete adapter and sensing power available. */
    bool ready;     /* Initial valid pair obtained; retries retain last values. */
    bool fresh;     /* Matched completed pair this tick, including invalidity. */
    bool battery_valid;
    bool tip_valid;
    bool invalid_battery;
    bool invalid_tip;
    bool acquisition_fault;
    uint8_t battery_failures;
    uint8_t tip_failures;
    uint32_t battery_mv;
    int32_t tip_mdegc;
    uint32_t filtered_battery_mv;
    int32_t filtered_tip_mdegc;
} Sensors_Snapshot;

/* Initialize once before acquisition; adc/ops must outlive the module. */
void Sensors_Init(ADC_HandleTypeDef *adc, const Sensors_AcquisitionOps *ops);
/* Foreground, called each app tick with current power availability. */
void Sensors_Update(bool power_available);
Sensors_Snapshot Sensors_GetSnapshot(void);
/* True only when no ADC/DMA ownership or stop/drain remains. Foreground only. */
bool Sensors_IsQuiescent(void);
/* ISR adapter forwards HAL half/full callbacks, normally with CHANNEL_ALL.
 * The acquired mask allows adapter-reported channel failures to stay isolated.
 * There is deliberately no epoch token: stop_quiescent drains old callbacks. */
void Sensors_OnDmaHalf(ADC_HandleTypeDef *adc, uint8_t acquired_channels);
void Sensors_OnDmaFull(ADC_HandleTypeDef *adc, uint8_t acquired_channels);
void Sensors_OnDmaError(ADC_HandleTypeDef *adc);

#endif /* USER_SENSORS_H */
