#include "pid.h"

void PID_Reset(PID_Controller *pid)
{
    *pid = (PID_Controller){0};
}

uint32_t PID_Update(PID_Controller *pid, int32_t error_mdegc, uint32_t dt_ms,
                    uint32_t min_mw, uint32_t max_mw)
{
    if (min_mw > max_mw) {
        min_mw = max_mw;
    }
    int64_t proportional = (int64_t)PID_KP_MW_PER_DEGC * error_mdegc *
                           (PID_POWER_FRACTION_SCALE / PID_MDEGC_PER_DEGC);
    if (error_mdegc < 0) {
        proportional *= PID_NEGATIVE_ERROR_KP_MULTIPLIER;
    }
    int64_t derivative = 0;
    if (dt_ms != 0U && pid->previous_error_valid) {
        derivative = (int64_t)PID_KD_MW_SECOND_PER_DEGC *
                     ((int64_t)error_mdegc - pid->previous_error_mdegc) *
                     PID_POWER_FRACTION_SCALE / dt_ms;
    }
    int64_t increment = (int64_t)PID_KI_MW_PER_DEGC_SECOND * error_mdegc *
                        dt_ms * (PID_POWER_FRACTION_SCALE /
                        (PID_MDEGC_PER_DEGC * PID_MS_PER_SECOND));
    int64_t lower = (int64_t)min_mw * PID_POWER_FRACTION_SCALE;
    int64_t upper = (int64_t)max_mw * PID_POWER_FRACTION_SCALE;
    int64_t candidate = proportional + pid->integral_fraction_mw +
                        increment + derivative;
    /* Reject integration further into either real actuator limit, but permit
     * increments directed out of saturation when the live ceiling decreases. */
    if (!((candidate > upper && increment > 0) ||
          (candidate < lower && increment < 0))) {
        pid->integral_fraction_mw += increment;
    }
    int64_t output = proportional + pid->integral_fraction_mw + derivative;
    pid->previous_error_mdegc = error_mdegc;
    pid->previous_error_valid = true;
    if (output < lower) {
        output = lower;
    }
    if (output > upper) {
        output = upper;
    }
    return (uint32_t)(output / PID_POWER_FRACTION_SCALE);
}
