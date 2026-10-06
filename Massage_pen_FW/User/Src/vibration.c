#include "vibration.h"
#include <stddef.h>
#include <string.h>

typedef enum {
    OFF, ENTRY_STATUS, ENTRY_STOP, ENTRY_IDLE, CONFIGURE, RESTORE,
    READY, CAL_CLEAR, CAL_MODE, CAL_START, CAL_WAIT, CAL_POLL_STATUS, CAL_STATUS,
    CAPTURE_COMP, CAPTURE_BEMF, CAPTURE_GAIN, RETRY_STOP, RETRY_IDLE,
    RTP_MODE, RTP_LEVEL, RTP_START, RTP_VERIFY, RTP_RUNNING, RUN_STATUS,
    CANCEL_DRAIN, CANCEL_STOP, CANCEL_IDLE, FAULT
} State;
typedef enum { READ, STATUS, CONFIG, CONFIG_IDLE, COMMAND } Operation;
typedef struct { uint8_t reg, mask, value; bool idle; } Setting;
static Vibration_Profile profile;
static Vibration_Observation observation;
static State state;
static bool pending, was_powered, cancel_to_off;
static uint8_t setting_index, pending_level, staged_comp, staged_bemf, attempt_status;
static uint32_t powered_ms, started_ms, polled_ms, stop_ms;
static I2C_DeviceResult completed;

static bool valid_profile(const Vibration_Profile *p)
{
    if (p == NULL || !p->validated) return false;
    uint32_t duration;
    switch (p->auto_cal_time) {
    case DRV2624_AUTO_CAL_TIME_250_MS: duration = VIBRATION_CAL_DURATION_250_MS; break;
    case DRV2624_AUTO_CAL_TIME_500_MS: duration = VIBRATION_CAL_DURATION_500_MS; break;
    case DRV2624_AUTO_CAL_TIME_1000_MS: duration = VIBRATION_CAL_DURATION_1000_MS; break;
    default: return false; /* Trigger-controlled completion remains unselected. */
    }
    return (p->mode & (DRV2624_TRIG_PIN_FUNC_MASK | DRV2624_MODE_MASK)) ==
               (DRV2624_TRIG_PIN_FUNC_INTERRUPT << DRV2624_TRIG_PIN_FUNC_SHIFT) &&
           (p->control & ~DRV2624_CONTROL_WRITABLE_MASK) == 0 &&
           (p->control & (DRV2624_LRA_ERM_MASK | DRV2624_CONTROL_LOOP_MASK |
                          DRV2624_HYBRID_LOOP_MASK)) == DRV2624_LRA_ERM_MASK &&
           p->rated_voltage != 0 && p->od_clamp != 0 &&
           (p->lra_drive_control & ~DRV2624_LRA_DRIVE_CONTROL_WRITABLE_MASK) == 0 &&
           (p->timing_control & ~DRV2624_TIMING_CONTROL_WRITABLE_MASK) == 0 &&
           p->calibration_duration_ms == duration &&
           p->calibration_timeout_ms > duration && p->calibration_timeout_ms <= INT32_MAX &&
           p->rtp[0] == 0 && p->rtp[1] > 0 && p->rtp[1] < p->rtp[2] &&
           p->rtp[2] < p->rtp[3] && p->rtp[3] <= DRV2624_RTP_INPUT_MAX;
}

void Vibration_Init(const Vibration_Profile *p)
{
    memset(&profile, 0, sizeof profile);
    observation = (Vibration_Observation){.profile_valid = valid_profile(p)};
    if (observation.profile_valid) profile = *p;
    state = OFF; pending = was_powered = cancel_to_off = false;
    setting_index = pending_level = staged_comp = staged_bemf = attempt_status = 0;
    powered_ms = started_ms = polled_ms = stop_ms = 0;
    completed = (I2C_DeviceResult){0};
}

static void fault_communication(void)
{
    DRV2624_Cancel(); /* App may remove power as soon as this fault is published. */
    observation.communication_fault = true;
    observation.configuration_ready = observation.calibration_ready = false;
    observation.output_active = false; observation.applied_level = 0;
    state = FAULT;
}

/* One retained driver operation at a time. An idle-register lease is maintained
 * by states reached only after GO=0; no state starts activity during retries or
 * abort draining. Terminal health alone never releases that lease. */
static bool operation(Operation kind, uint8_t reg, uint8_t mask, uint8_t value)
{
    if (!pending) {
        bool accepted;
        switch (kind) {
        case READ: accepted = DRV2624_RequestRead(reg); break;
        case STATUS: accepted = DRV2624_RequestStatus(); break;
        case CONFIG: accepted = DRV2624_RequestConfig(reg, mask, value); break;
        case CONFIG_IDLE: accepted = DRV2624_RequestConfigIdle(reg, mask, value); break;
        default: accepted = DRV2624_RequestCommand(reg, mask, value); break;
        }
        if (!accepted) { fault_communication(); return false; }
        pending = true; return false;
    }
    I2C_DeviceResult r = DRV2624_GetResult();
    if (r.exhausted) { fault_communication(); return false; }
    if (r.state == I2C_RESULT_PENDING || r.recovering) return false;
    bool success = (kind == READ || kind == STATUS) ? r.state == I2C_RESULT_READ :
                   kind == COMMAND ? (r.state == I2C_RESULT_COMMAND_ACCEPTED ||
                                      r.state == I2C_RESULT_COMMAND_UNCERTAIN) :
                   r.state == I2C_RESULT_VERIFIED;
    if (!success || !DRV2624_Acknowledge()) { fault_communication(); return false; }
    completed = r; pending = false; return true;
}

static bool read_go(void) { return operation(READ, DRV2624_REG_GO, 0, 0); }
static bool command_go(uint8_t value)
{ return operation(COMMAND, DRV2624_REG_GO, DRV2624_GO_MASK, value); }
static bool read_status(void) { return operation(STATUS, 0, 0, 0); }
static bool mode(uint8_t value)
{
    return operation(CONFIG, DRV2624_REG_MODE, DRV2624_MODE_MASK,
                     value << DRV2624_MODE_SHIFT);
}
static bool idle_observed(void) { return (completed.value & DRV2624_GO_MASK) == 0; }

static bool stopped_and_settled(uint32_t now)
{
    return idle_observed() && (!(profile.control & DRV2624_AUTO_BRK_INTO_STBY_MASK) ||
           (uint32_t)(now - stop_ms) >= VIBRATION_STOP_SETTLE_MS);
}

static Setting configuration(uint8_t i)
{
    const Setting settings[] = {
        {DRV2624_REG_MODE, DRV2624_MODE_WRITABLE_MASK, profile.mode, false},
        {DRV2624_REG_CONTROL, DRV2624_CONTROL_WRITABLE_MASK, profile.control, false},
        {DRV2624_REG_FEEDBACK_CONTROL, DRV2624_FEEDBACK_CONTROL_WRITABLE_MASK, profile.feedback_control, true},
        {DRV2624_REG_RATED_VOLTAGE, DRV2624_RATED_VOLTAGE_WRITABLE_MASK, profile.rated_voltage, false},
        {DRV2624_REG_OD_CLAMP, DRV2624_OD_CLAMP_WRITABLE_MASK, profile.od_clamp, false},
        {DRV2624_REG_LRA_DRIVE_CONTROL, DRV2624_LRA_DRIVE_CONTROL_WRITABLE_MASK, profile.lra_drive_control, false},
        {DRV2624_REG_BEMF_TIMING, DRV2624_BEMF_TIMING_WRITABLE_MASK, profile.bemf_timing, false},
        {DRV2624_REG_TIMING_CONTROL, DRV2624_TIMING_CONTROL_WRITABLE_MASK, profile.timing_control, false},
        {DRV2624_REG_AUTO_CAL_TIME, DRV2624_AUTO_CAL_TIME_WRITABLE_MASK, profile.auto_cal_time, false}
    };
    if (i >= sizeof settings / sizeof settings[0]) return (Setting){0};
    return settings[i];
}
static Setting restoration(uint8_t i)
{
    const Setting settings[] = {
        {DRV2624_REG_A_CAL_COMP, DRV2624_A_CAL_COMP_MASK, observation.a_cal_comp, true},
        {DRV2624_REG_A_CAL_BEMF, DRV2624_A_CAL_BEMF_MASK, observation.a_cal_bemf, true},
        {DRV2624_REG_FEEDBACK_CONTROL, DRV2624_BEMF_GAIN_MASK, observation.bemf_gain, true}
    };
    if (i >= sizeof settings / sizeof settings[0]) return (Setting){0};
    return settings[i];
}
static void calibration_failed(uint32_t now)
{
    /* Even on timeout/uncertain start, stop and observe GO=0 before another
     * calibration or any host write to the autonomously updated register. */
    state = RETRY_STOP; stop_ms = now;
}
static bool is_calibrating(void)
{ return state >= CAL_CLEAR && state <= RETRY_IDLE; }
static void cancel(bool to_off, uint32_t now)
{
    DRV2624_Cancel(); state = CANCEL_DRAIN; cancel_to_off = to_off; stop_ms = now;
    observation.output_active = false; observation.applied_level = 0;
}

void Vibration_Update(bool power_available, bool enabled, uint8_t requested_level)
{
    uint32_t now = HAL_GetTick();
    if (requested_level >= VIBRATION_LEVEL_COUNT) requested_level = 0;
    bool cancelling = state >= CANCEL_DRAIN && state <= CANCEL_IDLE;
    if (!power_available) {
        observation.configuration_ready = observation.calibration_ready = false;
        observation.output_active = false; observation.applied_level = 0;
        if (state != OFF && state != FAULT && state != CANCEL_DRAIN) cancel(true, now);
        cancel_to_off = true; was_powered = false;
    } else if (!was_powered) {
        was_powered = true; powered_ms = now;
    }
    if (power_available && state != OFF && state != FAULT && !cancelling &&
        (!enabled || (requested_level == 0 && (is_calibrating() ||
                         state == RTP_MODE || state == RTP_LEVEL ||
                         state == RTP_START || state == RTP_VERIFY || state == RTP_RUNNING || state == RUN_STATUS)))) {
        cancel(!enabled, now);
    }
    DRV2624_Update();
    observation.status = DRV2624_GetStatus();
    /* Keep protection evidence locally even if the app later acknowledges the
     * shared snapshot. DIAG_RESULT is an attempt result, not a controller fault. */
    if (observation.status.valid && (observation.status.value & VIBRATION_ERROR_MASK)) {
        observation.driver_fault = true;
        observation.configuration_ready = observation.calibration_ready = false;
        observation.output_active = false; observation.applied_level = 0;
        DRV2624_Cancel(); state = FAULT;
    }
    if (DRV2624_GetResult().exhausted) fault_communication();
    if (state == FAULT) return; /* Keep calling driver Update to drain ownership. */

    switch (state) {
    case OFF:
        if (power_available && enabled && observation.profile_valid &&
            (uint32_t)(now - powered_ms) >= DRV2624_POWER_UP_WAIT_MS) state = ENTRY_STATUS;
        break;
    case ENTRY_STATUS:
        if (read_status()) state = ENTRY_STOP;
        break;
    case ENTRY_STOP:
        if (command_go(0)) { state = ENTRY_IDLE; stop_ms = now; }
        break;
    case ENTRY_IDLE:
        if (read_go() && stopped_and_settled(now)) { state = CONFIGURE; setting_index = 0; }
        else if ((uint32_t)(now - stop_ms) >= profile.calibration_timeout_ms) fault_communication();
        break;
    case CONFIGURE: {
        Setting s = configuration(setting_index);
        if (!s.mask) {
            observation.configuration_ready = true; setting_index = 0;
            state = observation.calibration_retained ? RESTORE : READY;
        } else if (operation(s.idle ? CONFIG_IDLE : CONFIG, s.reg, s.mask, s.value)) setting_index++;
        break;
    }
    case RESTORE: {
        Setting s = restoration(setting_index);
        if (!s.mask) { observation.calibration_ready = true; state = READY; }
        else if (operation(CONFIG_IDLE, s.reg, s.mask, s.value)) setting_index++;
        break;
    }
    case READY:
        if (requested_level) {
            if (observation.calibration_ready) state = RTP_MODE;
            else if (observation.calibration_attempts >= VIBRATION_CALIBRATION_ATTEMPT_LIMIT) {
                observation.calibration_fault = true; state = FAULT;
            } else state = CAL_CLEAR;
        } else if ((uint32_t)(now - polled_ms) >= VIBRATION_STATUS_INTERVAL_MS) state = RUN_STATUS;
        break;
    case CAL_CLEAR:
        if (read_status()) { attempt_status = 0; state = CAL_MODE; } /* fresh attempt boundary */
        break;
    case CAL_MODE:
        if (mode(DRV2624_MODE_AUTO_CALIBRATION)) state = CAL_START;
        break;
    case CAL_START:
        if (!pending) {
            /* Count a possible physical command once, including uncertain ACK.
             * Never replay the command; subsequent states reconcile activity. */
            observation.calibration_attempts++; started_ms = now;
        }
        if (command_go(1)) { state = CAL_WAIT; polled_ms = now; }
        break;
    case CAL_WAIT:
        if (pending || (uint32_t)(now - polled_ms) >= VIBRATION_STATUS_INTERVAL_MS) {
            if (read_go()) {
                polled_ms = now;
                state = idle_observed() ? CAL_STATUS : CAL_POLL_STATUS;
            }
        }
        if (state == CAL_WAIT && !pending &&
            (uint32_t)(now - started_ms) >= profile.calibration_timeout_ms) calibration_failed(now);
        break;
    case CAL_POLL_STATUS:
        if (read_status()) {
            /* The motor can finish between the GO observation and STATUS read.
             * Retain only observations acquired since this attempt started. */
            attempt_status |= completed.value; state = CAL_WAIT;
        }
        break;
    case CAL_STATUS:
        if (read_status()) {
            attempt_status |= completed.value;
            if ((attempt_status & DRV2624_PROCESS_DONE_MASK) &&
                !(attempt_status & DRV2624_DIAG_RESULT_MASK) &&
                (uint32_t)(now - started_ms) >= profile.calibration_duration_ms) state = CAPTURE_COMP;
            else calibration_failed(now);
        }
        break;
    case CAPTURE_COMP:
        if (operation(READ, DRV2624_REG_A_CAL_COMP, 0, 0)) {
            staged_comp = completed.value; state = CAPTURE_BEMF;
        }
        break;
    case CAPTURE_BEMF:
        if (operation(READ, DRV2624_REG_A_CAL_BEMF, 0, 0)) {
            staged_bemf = completed.value; state = CAPTURE_GAIN;
        }
        break;
    case CAPTURE_GAIN:
        if (operation(READ, DRV2624_REG_FEEDBACK_CONTROL, 0, 0)) {
            observation.a_cal_comp = staged_comp; observation.a_cal_bemf = staged_bemf;
            observation.bemf_gain = completed.value & DRV2624_BEMF_GAIN_MASK;
            observation.calibration_retained = observation.calibration_ready = true;
            state = RTP_MODE;
        }
        break;
    case RETRY_STOP:
        if (command_go(0)) { state = RETRY_IDLE; stop_ms = now; }
        break;
    case RETRY_IDLE:
        if (read_go() && stopped_and_settled(now)) {
            if (observation.calibration_attempts >= VIBRATION_CALIBRATION_ATTEMPT_LIMIT) {
                observation.calibration_fault = true; state = FAULT;
            } else state = CAL_CLEAR;
        } else if ((uint32_t)(now - stop_ms) >= profile.calibration_timeout_ms) fault_communication();
        break;
    case RTP_MODE:
        if (mode(DRV2624_MODE_RTP)) state = RTP_LEVEL;
        break;
    case RTP_LEVEL:
        if (!pending) pending_level = requested_level;
        if (operation(CONFIG, DRV2624_REG_RTP_INPUT, DRV2624_RTP_INPUT_MASK, profile.rtp[pending_level])) {
            observation.applied_level = pending_level;
            state = observation.output_active ? RTP_RUNNING : RTP_START;
        }
        break;
    case RTP_START:
        if (command_go(1)) state = RTP_VERIFY;
        break;
    case RTP_VERIFY:
        /* UNCERTAIN retains a write buffer, not necessarily a physical state
         * observation. Always acquire GO freshly before publishing output. */
        if (read_go()) {
            if (!idle_observed()) { observation.output_active = true; state = RTP_RUNNING; }
            else fault_communication();
        }
        break;
    case RTP_RUNNING:
        /* Protection polling must progress even while every applied RTP level
         * is immediately replaced. Finish the current verified transaction,
         * then service overdue status before accepting another level update. */
        if ((uint32_t)(now - polled_ms) >= VIBRATION_STATUS_INTERVAL_MS) state = RUN_STATUS;
        else if (requested_level != observation.applied_level) state = RTP_LEVEL;
        break;
    case RUN_STATUS:
        if (read_status()) { polled_ms = now; state = observation.output_active ? RTP_RUNNING : READY; }
        break;
    case CANCEL_DRAIN: {
        I2C_DeviceResult r = DRV2624_GetResult();
        if (r.state == I2C_RESULT_PENDING || r.recovering) break;
        if (r.state != I2C_RESULT_IDLE && !DRV2624_Acknowledge()) break;
        pending = false;
        if (!power_available) state = OFF;
        else state = CANCEL_STOP;
        break;
    }
    case CANCEL_STOP:
        if (command_go(0)) { state = CANCEL_IDLE; stop_ms = now; }
        break;
    case CANCEL_IDLE:
        if (read_go() && stopped_and_settled(now)) {
            if (cancel_to_off) {
                observation.configuration_ready = observation.calibration_ready = false;
                state = OFF;
            } else state = READY;
        } else if ((uint32_t)(now - stop_ms) >= profile.calibration_timeout_ms) fault_communication();
        break;
    case FAULT: break;
    }
}
Vibration_Observation Vibration_GetObservation(void) { return observation; }
