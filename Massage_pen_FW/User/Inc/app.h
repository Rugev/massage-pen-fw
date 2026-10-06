/* Foreground application policy; main supplies board mechanisms explicitly. */
#ifndef USER_APP_H
#define USER_APP_H
#include "charging.h"
#include "vibration.h"
#include "heater.h"

#define APP_FAULT_BATTERY_UNDERVOLTAGE     14U

#define APP_ENABLE_HOLD_MS                300U
#define APP_DISABLE_HOLD_MS               1000U
#define APP_CLICK_MIN_MS                  100U
#define APP_CLICK_MAX_MS                  500U
#define APP_SESSION_LIMIT_MS              (75U * 60U * 1000U)
#define APP_ZERO_LEVEL_LIMIT_MS           (5U * 60U * 1000U)
#define APP_CHARGING_START_LIMIT_MS        10000U
#define APP_FAULT_INITIAL_DISPLAY_MS      60000U
#define APP_FAULT_WAKE_DISPLAY_MS         10000U
#define APP_COLD_INFERENCE_MARGIN_MDEGC    2000L

typedef enum {
    APP_STARTUP, APP_WAKEUP, APP_SLEEP, APP_CHARGING, APP_NORMAL,
    APP_CHARGING_NORMAL, APP_CHARGING_RECOVERY, APP_LOW_BATTERY_NOTICE,
    APP_BATTERY_DISCONNECT, APP_FAULT_DISPLAY, APP_FAULT_SLEEP
} App_State;

typedef enum { APP_WAKE_POWER, APP_WAKE_CHARGER } App_WakeSource;

typedef struct {
    ADC_HandleTypeDef *adc;
    const Sensors_AcquisitionOps *sensors;
    I2C_HandleTypeDef *charger_i2c, *vibration_i2c;
    const I2C_DeviceOps *charger_transport, *vibration_transport;
    const Charging_Profile *charging;
    const Vibration_Profile *vibration;
    const Heater_PwmOps *heater_pwm;
    void *heater_context;
} App_Bindings;

typedef struct {
    App_State state;
    uint8_t heat_level, vibration_level, fault_code;
    uint32_t session_started_ms;
    bool charging_eligible; /* Current raw battery passes admission; faults inhibit. */
    uint32_t battery_sequence;
    bool sleep_requested, shutdown_pending, sleep_ready;
} App_Snapshot;

/* Reboot only: initializes mechanisms and clears the RAM fault/calibration latch.
 * Profiles/adapters require actual board validation; they do not resolve the
 * outstanding battery-only NTC freshness hardware-release prerequisite. */
void App_Init(const App_Bindings *bindings, uint32_t now_ms);
/* Call once per foreground millisecond using the current HAL tick as now_ms.
 * Bus availability is independent of
 * SYS_ON. Module owners continue running while shutdown/cancellation drains. */
void App_Update(uint32_t now_ms, bool charger_bus_available);
/* Call once on physical sleep exit. A power wake identifies a new press episode:
 * adopt its verified debounced timestamp, or rearm ordinary raw-input debouncing.
 * Charger wake retains cached button identity; a continuing shutdown hold cannot
 * enable again. Fault wake retains the fault and ignores ordinary press handling. */
void App_Wake(uint32_t now_ms, App_WakeSource source, bool verified_power_press,
              uint32_t press_started_ms);
/* Policy sleep states precede physical sleep: keep foreground work running
 * while shutdown_pending. sleep_ready requires observed transport/ADC drains
 * and verified charger watchdog sleep preparation, with both outputs off. */
App_Snapshot App_GetSnapshot(void);
#endif
