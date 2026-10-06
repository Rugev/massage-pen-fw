/* PID controller. */
#ifndef USER_PID_H
#define USER_PID_H

#include <stdbool.h>
#include <stdint.h>

/* PROVISIONAL bench gains, require measured tip response before final tuning. */
#define PID_KP_MW_PER_DEGC                      500L
#define PID_KI_MW_PER_DEGC_SECOND               20L
#define PID_KD_MW_SECOND_PER_DEGC               0L
#define PID_NEGATIVE_ERROR_KP_MULTIPLIER        2L
#define PID_POWER_FRACTION_SCALE                1000000LL
#define PID_MDEGC_PER_DEGC                      1000LL
#define PID_MS_PER_SECOND                      1000LL

typedef struct {
    int64_t integral_fraction_mw;
    int32_t previous_error_mdegc;
    bool previous_error_valid;
} PID_Controller;

void PID_Reset(PID_Controller *pid);
/* Accepted heater temperature errors and 50 ms cadence. Bounds are the actual
 * voltage/current-limited power, in mW. Zero dt does not integrate/derive. */
uint32_t PID_Update(PID_Controller *pid, int32_t error_mdegc, uint32_t dt_ms,
                    uint32_t min_mw, uint32_t max_mw);

#endif /* USER_PID_H */
