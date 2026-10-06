/* Vibration control behavior. */
#ifndef USER_VIBRATION_H
#define USER_VIBRATION_H

/* Linear resonant motor; driver frequency tracking is intended. */
#define VIBRATION_NOMINAL_FREQUENCY_HZ          70U
#define VIBRATION_MAX_RMS_MV                    2000U

/* Levels 0..3 correspond to 0..3 illuminated LEDs. */
#define VIBRATION_LEVEL_COUNT                   4U
#define VIBRATION_LEVEL_0_RMS_MV                0U
#define VIBRATION_LEVEL_1_RMS_MV                500U
#define VIBRATION_LEVEL_2_RMS_MV                1000U
#define VIBRATION_LEVEL_3_RMS_MV                2000U

#include "drv2624.h"
#define VIBRATION_CALIBRATION_ATTEMPT_LIMIT 3U
#define VIBRATION_STATUS_INTERVAL_MS 100U
/* Conservative physical braking bound covers both playback interval codes.
 * A fresh I2C access after this delay completes the documented standby exit. */
#define VIBRATION_STOP_SETTLE_MS (DRV2624_AUTO_BRAKE_TICK_COUNT * \
    DRV2624_PLAYBACK_TICK_SLOW_MS + DRV2624_AUTO_BRAKE_BUFFER_MS)
#define VIBRATION_CAL_DURATION_250_MS 250U
#define VIBRATION_CAL_DURATION_500_MS 500U
#define VIBRATION_CAL_DURATION_1000_MS 1000U
#define VIBRATION_ERROR_MASK (DRV2624_PRG_ERROR_MASK | DRV2624_UVLO_MASK | \
                              DRV2624_OVER_TEMP_MASK | DRV2624_OC_DETECT_MASK)

/* No board profile is supplied. validated asserts bench validation of the
 * complete motor/input configuration and RTP scaling below. All values are
 * explicit register encodings, excluding reserved bits. Software GO requires
 * interrupt trigger mode, LRA and full closed loop. Fixed-duration calibration
 * only; supply its duration and a validated bounded completion timeout.
 * Init copies the profile, clears retained calibration and requires the caller
 * to initialize DRV2624 first while no transfer/process owns the device. */
typedef struct {
    bool validated;
    uint8_t mode, control, feedback_control, rated_voltage, od_clamp;
    uint8_t lra_drive_control, bemf_timing, timing_control, auto_cal_time;
    uint32_t calibration_duration_ms, calibration_timeout_ms;
    uint8_t rtp[VIBRATION_LEVEL_COUNT];
} Vibration_Profile;
typedef struct {
    bool profile_valid, configuration_ready, calibration_ready;
    bool calibration_retained, output_active;
    bool driver_fault, communication_fault, calibration_fault;
    uint8_t applied_level, calibration_attempts;
    uint8_t a_cal_comp, a_cal_bemf, bemf_gain;
    DRV2624_StatusSnapshot status;
} Vibration_Observation;
void Vibration_Init(const Vibration_Profile *profile);
/* Foreground owner of DRV2624 operations/Update. enabled means normal operation
 * entry; power_available must reflect a usable controller supply, including
 * every reset/brownout. False cancels/drains transport before reuse. Call every
 * foreground cycle even during shutdown. Sleep preserves RAM; Init is reboot.
 * App alone acknowledges shared STATUS after all consumers observe the sequence.
 * Aggregate status flags remain visible; calibration uses fresh raw reads. */
void Vibration_Update(bool power_available, bool enabled, uint8_t requested_level);
Vibration_Observation Vibration_GetObservation(void);

#endif /* USER_VIBRATION_H */
