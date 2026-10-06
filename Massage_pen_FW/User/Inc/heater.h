/* Heater control using PWM and temperature feedback. */
#ifndef USER_HEATER_H
#define USER_HEATER_H

/* NMOS-switched resistive heater: low control signal means off. */
#define HEATER_RESISTANCE_MOHM                  2000U
#define HEATER_CTRL_ACTIVE_HIGH                 1U
#define HEATER_ABSOLUTE_MAX_TEMP_MDEGC          49000L

/* Level 0 means off, not a temperature target. Levels map to 0..3 LEDs. */
#define HEATER_LEVEL_COUNT                      4U
#define HEATER_LEVEL_0_TEMP_MDEGC               0L
#define HEATER_LEVEL_1_TEMP_MDEGC               40000L
#define HEATER_LEVEL_2_TEMP_MDEGC               44000L
#define HEATER_LEVEL_3_TEMP_MDEGC               48000L

#include <stdbool.h>
#include <stdint.h>
#include "sensors.h"

#define HEATER_FAULT_TEMP_MDEGC                 49500L
#define HEATER_PHASE_BAND_MDEGC                 2000L
#define HEATER_MA_PER_A                          1000U
#define HEATER_DUTY_SCALE                       1000000U
#define HEATER_CONTROL_PERIOD_MS                50U
#define HEATER_AVERAGE_CURRENT_REDUCED_MA        1000U
#define HEATER_AVERAGE_CURRENT_NORMAL_MA         2000U
#define HEATER_REDUCED_CURRENT_TIP_MDEGC         10000L

/* Binding must already be safe and translate logical on-time to electrical
 * active-high PWM: 0 = low/off, DUTY_SCALE = continuously high. TIM1
 * CH1 must be PWM1 with HIGH polarity; the board adapter validates it.
 * NULL gates operation. The adapter/context must outlive the module. */
typedef struct {
    void (*set_duty)(void *context, uint32_t duty_ppm);
} Heater_PwmOps;

typedef enum {
    HEATER_OFF,
    HEATER_PREHEAT,
    HEATER_PRECOOL,
    HEATER_PID,
    HEATER_INHIBITED
} Heater_Phase;

typedef struct {
    bool available;
    bool active;
    bool fault_temperature; /* Latched until initialization at reboot. */
    Heater_Phase phase;
    int32_t target_mdegc;
    uint32_t power_mw;
    uint32_t duty_ppm;
} Heater_Snapshot;

/* Init commands zero to a complete binding; app readiness alone cannot bind it. */
void Heater_Init(const Heater_PwmOps *ops, void *context);
/* Foreground every app tick. App supplies authorization and live average current
 * ceiling (charger cool OR raw tip < 10 degC). Sensors retries retain controls.
 * Raw protection and duty/ceiling reconciliation run independently of PID. */
void Heater_Update(uint32_t now_ms, uint8_t requested_level, bool authorized,
                   uint32_t average_current_limit_ma,
                   const Sensors_Snapshot *sensors);
Heater_Snapshot Heater_GetSnapshot(void);

#endif /* USER_HEATER_H */
