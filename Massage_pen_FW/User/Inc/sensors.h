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

/* Public API and implementation remain undecided. */

#endif /* USER_SENSORS_H */
