/* Charging behavior and battery status. */
#ifndef USER_CHARGING_H
#define USER_CHARGING_H

#define BATTERY_CELL_COUNT                      1U
#define BATTERY_CAPACITY_MAH                    3000U
#define BATTERY_MIN_MV                          2750U
#define BATTERY_NOMINAL_MV                      3600U
#define BATTERY_MAX_MV                          4200U
#define BATTERY_MAX_CHARGE_CURRENT_MA           1400U

/* Cell specification limits; temperature units are mdegC. */
#define BATTERY_CHARGE_MIN_MDEGC                0
#define BATTERY_CHARGE_MAX_MDEGC                45000
#define BATTERY_DISCHARGE_MIN_MDEGC             (-20000)
#define BATTERY_DISCHARGE_MAX_MDEGC             65000
#define BATTERY_TERMINATION_CURRENT_MA          60U

/* Intended setpoints, not factory defaults; no registers are written yet.
 * Follow CC/D+/D- detection, applying the lower of the detected input limit
 * and this USB ceiling. Battery charge current is limited separately above.
 */
#define CHARGER_USB_INPUT_MAX_MA                1500U
#define CHARGER_SOURCE_DETECTION_ENABLED        1U
#define CHARGER_SYS_MIN_MV                      2975U

/* Pending manufacturer confirmation: verify both the pre-charge current and
 * threshold, and whether deeply discharged cells may be recovered at all.
 */
#define CHARGER_PRECHARGE_CURRENT_MA            20U
#define CHARGER_PRECHARGE_THRESHOLD_MV          2800U
/* Trickle charging is disabled by selecting zero current. Check this setting
 * and the permitted deep-discharge recovery procedure with the manufacturer.
 */
#define CHARGER_TRICKLE_CURRENT_MA              0U

/* NTC1 is the battery sensor; PG retains its power-good function.
 * Thresholds are nominal for the installed beta-3435 divider; hardware has
 * hysteresis. The MCU must pause charging on warm/hot status and resume only
 * below the warm threshold, if charging is still required and otherwise safe.
 */
#define CHARGER_NTC1_PROTECTION_ENABLED         1U
#define CHARGER_NTC2_PROTECTION_ENABLED         0U
#define CHARGER_COLD_THRESHOLD_MDEGC            5000
#define CHARGER_COOL_THRESHOLD_MDEGC            15000
#define CHARGER_COOL_CURRENT_PERCENT            33U
#define CHARGER_WARM_PAUSE_THRESHOLD_MDEGC      40000
/* Hardware fallback; exceeds the cell charging limit and does not replace
 * the MCU warm cutoff. Watchdog expiry restores the factory NTC thresholds.
 */
#define CHARGER_HOT_THRESHOLD_MDEGC             50000

/* Service while USB input is valid, including temperature pauses and the
 * charge-complete state awaiting recharge. Disable on battery-only operation
 * or before sleep; restore/check configuration following watchdog expiry.
 */
#define CHARGER_WATCHDOG_TIMEOUT_MS             40000U
#define CHARGER_WATCHDOG_SERVICE_INTERVAL_MS    10000U
#define CHARGER_WATCHDOG_ON_BATTERY_ONLY         0U
#define CHARGER_WATCHDOG_DURING_THERMAL_PAUSE    1U

#define CHARGER_BOOST_ENABLED                   0U
#define CHARGER_BATTFET_RESET_ENABLED           1U
#define CHARGER_SHIPPING_DELAY_ENABLED          0U
#define CHARGER_PARAMETER_LOCK_ENABLED          1U
/* Writable INT_MASK bits only: all maskable events enabled. Preserve the
 * reserved bits when constructing the register write.
 */
#define CHARGER_INTERRUPT_MASK_BITS             0U

#include <stdbool.h>
#include <stdint.h>
#define CHARGER_INTERRUPT_ACTIVE_HIGH          0U
#define CHARGING_POLL_INTERVAL_MS               100U
#define CHARGING_STATUS_MAX_AGE_MS              110U
#define CHARGING_CONFIG_REGISTER_COUNT          16U

/* Nonblocking board adapter reserves hardware autonomy for the whole register transaction.
 * IIN: block detection/restart/CC changes. CHG_CTRL3: prevent discharge/OCP.
 * Lease lasts through preread, retries, verification and cancellation drain.
 * Missing adapters leave readiness false; a VIN_RDY sample is not a lease. */
typedef struct {
    bool (*acquire)(uint8_t reg);
    void (*release)(uint8_t reg);
} Charging_IdleOps;
/* Explicit complete, reviewed board settings in MP2724 catalogue order (first
 * 16 entries, skipping undocumented 0x0B). Reserved/action bits must be zero.
 * No default board profile is supplied. Caller storage must outlive module. */
typedef struct {
    bool agreed;
    uint8_t registers[CHARGING_CONFIG_REGISTER_COUNT];
    const Charging_IdleOps *idle;
} Charging_Profile;
typedef struct {
    bool profile_valid, configuration_ready, status_ready;
    bool input_valid, input_ready, active_charging, topoff_active;
    uint8_t phase, ntc1, status[6];
    bool ntc_fresh, cold, hot, cool, warm, paused, completed;
    bool charger_fault, watchdog_fault, communication_fault;
    bool recovering, sleep_ready, shipping_requested, shipping_accepted, shipping_uncertain;
    bool idle_lease;
    uint8_t idle_register;
    uint32_t status_sequence, status_ms;
} Charging_Observation;
/* MP2724_Init is owned by startup and must precede this initialization. */
void Charging_Init(const Charging_Profile *profile);
/* Startup/ordinary wake only. Does not clear latched runtime faults.
 * Reverses PrepareSleep and invalidates status/configuration until reacquired. */
void Charging_BeginValidation(void);
void Charging_EndValidation(void);
/* Completed state survives a warm pause. Automatic recharge rearming after
 * warm/done inhibition needs separate agreed policy; this API adds none. */
void Charging_SetChargeRequired(bool required);
/* Sole foreground owner of MP2724 requests/Update; call every app cycle,
 * including unavailable power and shutdown, until cancellation drains. */
void Charging_Update(bool available);
void Charging_OnInterrupt(void);
void Charging_RequestPrepareSleep(void);
/* Deliberate immediate shipping action, at most once per request lifecycle.
 * Missing idle adapter leaves the request visibly pending without guessing
 * a shipping delay. Accepted reports transport ACK; uncertain cannot be replayed. */
bool Charging_RequestShipping(void);
Charging_Observation Charging_GetObservation(void);

#endif /* USER_CHARGING_H */
