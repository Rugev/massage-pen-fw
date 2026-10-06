#include "charging.h"
#include "mp2724.h"
#include <stddef.h>
#include <string.h>

typedef enum {
    OP_NONE, OP_STATUS, OP_CTRL4, OP_CTRL3, OP_CTRL2, OP_AUDIT,
    OP_CONFIG, OP_LOCK, OP_WATCHDOG, OP_SERVICE, OP_CHARGE,
    OP_SHIP_DELAY, OP_SHIP, OP_INPUT_AUDIT, OP_INPUT_CONFIG, OP_INHIBIT, OP_ZERO, OP_ARM_INHIBIT, OP_ARM_UNLOCK, OP_ARM_CURRENT, OP_ARM_LOCK, OP_ARM_ENABLE, OP_BASELINE_LOCK
} Operation;
static const Charging_Profile *profile;
static Charging_Observation observation;
static Operation operation;
static uint8_t config_index, config_mask, config_value, poll_index;
static uint8_t ctrl2, ctrl4;
static bool enable_known, ipre_known, lock_known, baseline_required, charge_target;
static uint8_t ipre, parameter_lock, ipre_target;
typedef enum {ARM_IDLE, ARM_INHIBIT, ARM_UNLOCK, ARM_CURRENT, ARM_LOCK, ARM_ENABLE} ArmStage;
static ArmStage arm_stage;
static bool battery_eligible, normal_operation, fault_inhibited, precharge_seen, phase_known;
static uint32_t battery_sequence, precharge_sequence, phase_ms;

static bool ctrl2_known, ctrl4_known, validation, need_config, lock_pending;
static bool charge_required, sleep_requested, shipping_requested;
static bool watchdog_running, service_due, recovering_watchdog;
static bool poll_active, initial_poll, cancel_requested, available_before;
static uint8_t shipping_stage;
static bool input_audit_due, config_write_due, input_write_due;
static uint32_t poll_started_ms, service_ms;
static volatile bool interrupt_pending;
/* Poll a complete fixed-order round. An interrupt never rewinds the cursor. */
static const uint8_t status_order[] = {1U, 2U, 3U, 0U, 4U, 5U};

static void refresh_baseline(void)
{
    observation.safe_baseline_ready = enable_known && !(ctrl4 & MP2724_EN_CHG_MASK) &&
        ipre_known && !(ipre & MP2724_IPRE_MASK);
}
static bool readiness_allowed(void)
{
    return !baseline_required && lock_known && (parameter_lock & MP2724_LOCK_CHG_MASK) &&
        observation.profile_valid && !need_config &&
        !observation.charger_fault && !observation.watchdog_fault &&
        !observation.communication_fault && !observation.cold && !observation.hot;
}
static uint8_t stable_mask(uint8_t index)
{
    const mp2724_register_info_t *info = &mp2724_registers[index];
    uint8_t mask = info->writable_mask & (uint8_t)~info->command_mask;
    if (info->address == MP2724_REG_CHG_CTRL3)
        mask &= (uint8_t)~MP2724_BATTFET_DIS_MASK;
    return mask;
}
static bool valid_profile(const Charging_Profile *p)
{
    if (p == NULL || !p->agreed) return false;
    for (uint8_t i = 0; i < CHARGING_CONFIG_REGISTER_COUNT; ++i)
        if ((p->registers[i] & (uint8_t)~stable_mask(i)) != 0U) return false;
    const uint8_t *v = p->registers;
    /* Enforce agreed limits separately from the still-open profile fields. */
    return (v[0] & (MP2724_LOCK_CHG_MASK | MP2724_EN_PG_NTC2_MASK)) == MP2724_LOCK_CHG_MASK &&
        (v[1] & MP2724_IIN_MODE_MASK) == 0U &&
        MP2724_FIELD_GET(MP2724_ICC_MASK, MP2724_ICC_SHIFT, v[2]) * MP2724_ICC_STEP_MA <= BATTERY_MAX_CHARGE_CURRENT_MA &&
        MP2724_FIELD_GET(MP2724_VPRE_MASK, MP2724_VPRE_SHIFT, v[2]) == MP2724_VPRE_3000_MV &&
        MP2724_FIELD_GET(MP2724_IPRE_MASK, MP2724_IPRE_SHIFT, v[3]) * MP2724_IPRE_STEP_MA == CHARGER_PRECHARGE_OPERATING_CURRENT_MA &&
        MP2724_FIELD_GET(MP2724_ITERM_MASK, MP2724_ITERM_SHIFT, v[3]) * MP2724_ITERM_STEP_MA + MP2724_ITERM_OFFSET_MA == BATTERY_TERMINATION_CURRENT_MA &&
        (v[4] & MP2724_ITRICKLE_MASK) == 0U &&
        MP2724_FIELD_GET(MP2724_VBATT_MASK, MP2724_VBATT_SHIFT, v[5]) * MP2724_VBATT_STEP_MV + MP2724_VBATT_OFFSET_MV == BATTERY_MAX_MV &&
        (v[6] & MP2724_SYS_MIN_MASK) == 0U && (v[6] & MP2724_TREG_MASK) <= MP2724_TREG_120_C &&
        (v[7] & (MP2724_WATCHDOG_MASK | MP2724_EN_TERM_MASK)) == (MP2724_WATCHDOG_40_S << MP2724_WATCHDOG_SHIFT | MP2724_EN_TERM_MASK) &&
        (v[8] & (MP2724_BATTFET_DLY_MASK | MP2724_BATTFET_RST_EN_MASK)) == MP2724_BATTFET_RST_EN_MASK &&
        (v[9] & (MP2724_EN_BOOST_MASK | MP2724_CC_CFG_MASK)) == 0U &&
        (v[10] & (MP2724_AUTODPDM_MASK | MP2724_FORCE_CC_MASK)) == MP2724_AUTODPDM_MASK &&
        (v[11] & (MP2724_NTC1_ACTION_MASK | MP2724_NTC2_ACTION_MASK)) == MP2724_NTC1_ACTION_MASK &&
        (v[12] & (MP2724_COOL_ACT_MASK | MP2724_JEITA_ISET_MASK)) == (MP2724_COOL_ACT_REDUCE_CURRENT << MP2724_COOL_ACT_SHIFT | MP2724_JEITA_ISET_33_PERCENT) &&
        v[13] == (MP2724_VHOT_259_PERMILLE << MP2724_VHOT_SHIFT |
                  MP2724_VWARM_365_PERMILLE << MP2724_VWARM_SHIFT |
                  MP2724_VCOOL_599_PERMILLE << MP2724_VCOOL_SHIFT |
                  MP2724_VCOLD_696_PERMILLE << MP2724_VCOLD_SHIFT) &&
        v[15] == CHARGER_INTERRUPT_MASK_BITS;
}
static bool acquire_idle(uint8_t reg)
{
    if (observation.idle_lease) return observation.idle_register == reg;
    if (profile == NULL || profile->idle == NULL || profile->idle->acquire == NULL ||
        profile->idle->release == NULL || !profile->idle->acquire(reg)) return false;
    observation.idle_lease = true;
    observation.idle_register = reg;
    return true;
}
static void release_idle(void)
{
    if (!observation.idle_lease) return;
    profile->idle->release(observation.idle_register);
    observation.idle_lease = false;
}
static bool read_register(uint8_t reg, Operation next)
{
    if (!MP2724_RequestRead(reg)) return false;
    observation.sleep_ready = false;
    operation = next;
    return true;
}
static bool configure(uint8_t reg, uint8_t mask, uint8_t value, Operation next)
{
    bool idle = reg == MP2724_REG_IIN || reg == MP2724_REG_CHG_CTRL3;
    if (idle && !acquire_idle(reg)) return false;
    bool accepted = idle ? MP2724_RequestConfigIdle(reg, mask, value) : MP2724_RequestConfig(reg, mask, value);
    if (!accepted) { if (idle) release_idle(); return false; }
    if (reg == MP2724_REG_CHG_CTRL2) ctrl2_known = false;
    if (reg == MP2724_REG_CHG_CTRL4) enable_known = ctrl4_known = false;
    if (reg == MP2724_REG_CHG_PARAMETER1) ipre_known = false;
    if (reg == MP2724_REG_CHG_CTRL0) lock_known = false;
    refresh_baseline();
    observation.sleep_ready = false;
    operation = next;
    return true;
}
static bool command(uint8_t reg, uint8_t mask, Operation next)
{
    if (!MP2724_RequestCommand(reg, mask, mask)) return false;
    if (reg == MP2724_REG_CHG_CTRL2) ctrl2_known = false;
    observation.sleep_ready = false;
    operation = next;
    return true;
}
static void record_shipping_outcome(I2C_ResultState state)
{
    observation.shipping_accepted = state == I2C_RESULT_COMMAND_ACCEPTED;
    observation.shipping_uncertain = !observation.shipping_accepted;
    shipping_stage = 3U;
}
static void cancel_operation(void)
{
    if (operation == OP_NONE) return;
    /* Dispatch of a side-effect command closes its lifecycle even if cancel
     * prevents the observation. A pending/uncertain write cannot be replayed. */
    if (operation == OP_SHIP) record_shipping_outcome(MP2724_GetResult().state);
    ctrl2_known = enable_known = ipre_known = lock_known = ctrl4_known = false;
    baseline_required = true; charge_target = false;
    ipre_target = 0U; arm_stage = ARM_IDLE; precharge_seen = phase_known = false; observation.admitted = false;
    refresh_baseline();
    observation.sleep_ready = false;
    MP2724_Cancel();
    cancel_requested = true;
}
static bool desired_charge(void)
{
    return charge_required && battery_eligible && !fault_inhibited &&
        observation.status_ready && observation.ntc_fresh &&
        HAL_GetTick() - observation.status_ms <= CHARGING_STATUS_MAX_AGE_MS &&
        observation.configuration_ready && observation.input_ready &&
        !observation.warm && !observation.hot && !observation.cold &&
        !(observation.status[3] & (MP2724_NTC_MISSING_MASK | MP2724_BATT_MISSING_MASK)) &&
        !observation.charger_fault && !observation.watchdog_fault && !observation.communication_fault;
}
static void decode_status(uint8_t index, uint8_t value)
{
    observation.status[index] = value;
    if (index == 1U) {
        bool was_valid = observation.input_valid;
        observation.input_valid = (value & MP2724_VIN_GD_MASK) != 0U;
        observation.input_ready = observation.input_valid && (value & MP2724_VIN_RDY_MASK) != 0U;
        if (!observation.input_valid) {
            observation.ntc_fresh = false;
            observation.cool = observation.warm = false;
            observation.completed = false;
        }
        if (was_valid != observation.input_valid) {
            need_config = observation.profile_valid;
            input_audit_due = false;
            config_index = 0U;
            observation.configuration_ready = false;
        }
        if (value & MP2724_WATCHDOG_FAULT_MASK) {
            enable_known = ipre_known = lock_known = false;
            baseline_required = true; charge_target = false;
            ipre_target = 0U; arm_stage = ARM_IDLE; precharge_seen = phase_known = false; observation.admitted = false; refresh_baseline();
            observation.configuration_ready = false;
            if (validation) {
                if (!recovering_watchdog) {need_config = observation.profile_valid; config_index = 0U;}
                recovering_watchdog = true;
                service_due = true;
            } else observation.watchdog_fault = true;
        }
        if (value & MP2724_WATCHDOG_BARK_MASK) service_due = true;
    } else if (index == 2U) {
        phase_known = true; phase_ms = HAL_GetTick();
        observation.phase = MP2724_FIELD_GET(MP2724_CHG_STAT_MASK, MP2724_CHG_STAT_SHIFT, value);
        observation.active_charging = observation.phase >= MP2724_CHG_STAT_TRICKLE &&
            observation.phase <= MP2724_CHG_STAT_CONSTANT_VOLTAGE;
        if (observation.admitted && arm_stage == ARM_IDLE) {
            if (observation.phase == MP2724_CHG_STAT_PRECHARGE) {
                if (!precharge_seen) {precharge_seen = true; precharge_sequence = battery_sequence;}
            } else precharge_seen = false;
        }
        if (observation.phase == MP2724_CHG_STAT_DONE) observation.completed = true;

        if (value & (MP2724_CHG_FAULT_MASK | MP2724_BOOST_FAULT_MASK)) observation.charger_fault = true;
    } else if (index == 3U) {
        observation.ntc1 = MP2724_FIELD_GET(MP2724_NTC1_FAULT_MASK, MP2724_NTC1_FAULT_SHIFT, value);
        /* Freshness is qualified by the converter observation later this round. */
        if (value & (MP2724_NTC_MISSING_MASK | MP2724_BATT_MISSING_MASK)) observation.charger_fault = true;
    } else if (index == 5U) observation.topoff_active = (value & MP2724_TOPOFF_ACTIVE_MASK) != 0U;
    if (observation.charger_fault || observation.watchdog_fault || observation.cold || observation.hot)
        observation.configuration_ready = false;
}
static void next_config(void)
{
    release_idle();
    config_index++;
    if (config_index == CHARGING_CONFIG_REGISTER_COUNT) lock_pending = true;
}
static uint8_t expected_config(uint8_t index, uint8_t current)
{
    uint8_t value = profile->registers[index];
    if (index == 0U) value &= (uint8_t)~MP2724_LOCK_CHG_MASK;
    if (index == 1U) {
        uint8_t code = current & MP2724_IIN_LIM_MASK;
        uint8_t ceiling = (CHARGER_USB_INPUT_MAX_MA - MP2724_IIN_LIM_OFFSET_MA) / MP2724_IIN_LIM_STEP_MA;
        value = code < ceiling ? code : ceiling;
    }
    if (index == 3U) value = (value & (uint8_t)~MP2724_IPRE_MASK) | ipre_target;
    if (index == 7U) {
        value &= (uint8_t)~MP2724_WATCHDOG_MASK;
        if (observation.input_valid) value |= MP2724_WATCHDOG_40_S << MP2724_WATCHDOG_SHIFT;
    }
    if (index == 9U) {
        /* Auditing an already-completed or paused cycle never enables it. */
        value = (value & (uint8_t)~MP2724_EN_CHG_MASK) | (charge_target ? MP2724_EN_CHG_MASK : 0U);
    }
    return value;
}
static void consume_result(I2C_DeviceResult result)
{
    if (operation == OP_NONE || result.state == I2C_RESULT_PENDING || result.state == I2C_RESULT_IDLE) return;
    if (result.exhausted) {
        /* Permission ends at health exhaustion; physical ownership still drains. */
        charge_target = false; observation.admitted = false;
        ipre_target = 0U; arm_stage = ARM_IDLE;
        precharge_seen = phase_known = false; baseline_required = true;
        enable_known = ipre_known = lock_known = false; refresh_baseline();
        observation.communication_fault = true;
        observation.configuration_ready = false;
        observation.ntc_fresh = false;
    }
    if (operation == OP_SHIP) record_shipping_outcome(result.state);
    if (result.recovering) return; /* Retain lease and callback buffer even after terminal health failure. */
    Operation done = operation;
    operation = OP_NONE;
    if (!MP2724_Acknowledge()) return;
    if (done == OP_SHIP) {
        cancel_requested = false;
        if (result.state == I2C_RESULT_FAILED || result.state == I2C_RESULT_REJECTED)
            observation.communication_fault = true;
        return;
    }
    if (result.state == I2C_RESULT_CANCELLED || cancel_requested) {
        cancel_requested = false;
        release_idle();
        return;
    }
    if (result.state == I2C_RESULT_FAILED || result.state == I2C_RESULT_REJECTED) {
        enable_known = ipre_known = lock_known = false; refresh_baseline();
        observation.communication_fault = true;
        observation.configuration_ready = false;
        observation.ntc_fresh = false;
        release_idle();
        return;
    }
    switch (done) {
    case OP_BASELINE_LOCK:
        parameter_lock = result.value; lock_known = true;
        if (observation.safe_baseline_ready) baseline_required = false;
        break;
    case OP_ARM_INHIBIT:
        ctrl4 = result.value; ctrl4_known = enable_known = true; refresh_baseline();
        arm_stage = ARM_UNLOCK;
        break;
    case OP_ARM_UNLOCK:
        parameter_lock = result.value; lock_known = true; arm_stage = ARM_CURRENT;
        break;
    case OP_ARM_CURRENT:
        ipre = result.value; ipre_known = true; refresh_baseline(); arm_stage = ARM_LOCK;
        break;
    case OP_ARM_LOCK:
        parameter_lock = result.value; lock_known = true; arm_stage = ARM_ENABLE;
        break;
    case OP_ARM_ENABLE:
        ctrl4 = result.value; ctrl4_known = enable_known = true; refresh_baseline();
        observation.admitted = true; arm_stage = ARM_IDLE;
        break;
    case OP_INHIBIT:
        observation.paused = observation.warm;
        ctrl4 = result.value; ctrl4_known = enable_known = true;
        refresh_baseline();
        break;
    case OP_ZERO:
        ipre = result.value; ipre_known = true;
        refresh_baseline();
        if (observation.safe_baseline_ready && lock_known && (parameter_lock & MP2724_LOCK_CHG_MASK))
            baseline_required = false;
        break;
    case OP_STATUS:
        decode_status(status_order[poll_index], result.value);
        poll_index++;
        break;
    case OP_CTRL4:
        ctrl4 = result.value; ctrl4_known = enable_known = true; refresh_baseline(); poll_index++;
        observation.ntc_fresh = observation.input_ready && (ctrl4 & MP2724_EN_BUCK_MASK) != 0U;
        observation.cool = observation.ntc_fresh && observation.ntc1 == MP2724_NTC1_FAULT_COOL;
        observation.warm = observation.ntc_fresh && observation.ntc1 == MP2724_NTC1_FAULT_WARM;
        if (observation.ntc_fresh && observation.ntc1 == MP2724_NTC1_FAULT_COLD) observation.cold = true;
        if (observation.ntc_fresh && observation.ntc1 == MP2724_NTC1_FAULT_HOT) observation.hot = true;
        if (observation.cold || observation.hot) observation.configuration_ready = false;
        break;
    case OP_CTRL3:
        if (result.value & MP2724_BATTFET_DIS_MASK) {
            observation.charger_fault = true;
            observation.configuration_ready = false;
        }
        poll_index++;
        break;
    case OP_CTRL2:
        ctrl2 = result.value; ctrl2_known = true; poll_index++;
        poll_active = false; initial_poll = false;
        observation.status_ready = true;
        observation.status_sequence++;
        observation.status_ms = HAL_GetTick();
        input_audit_due |= observation.configuration_ready && observation.input_ready;
        break;
    case OP_AUDIT:
        if (mp2724_registers[config_index].address == MP2724_REG_CHG_CTRL3 &&
            (result.value & MP2724_BATTFET_DIS_MASK)) {
            observation.charger_fault = true;
            observation.configuration_ready = false;
            release_idle();
            break;
        }
        config_mask = stable_mask(config_index);
        config_value = expected_config(config_index, result.value);
        if (config_index == 9U) {ctrl4 = result.value; ctrl4_known = enable_known = true;}
        if (config_index == 3U) {ipre = result.value; ipre_known = true;}
        if (config_index == 0U) {parameter_lock = result.value; lock_known = true;}
        refresh_baseline();
        if (config_index == 7U) {ctrl2 = result.value; ctrl2_known = true;}
        if ((result.value & config_mask) == config_value) next_config();
        else config_write_due = true;
        break;
    case OP_CONFIG:
        if (config_index == 7U) {ctrl2 = result.value;ctrl2_known = true;}
        if (config_index == 9U) {ctrl4 = result.value; ctrl4_known = enable_known = true;}
        if (config_index == 3U) {ipre = result.value; ipre_known = true;}
        if (config_index == 0U) {parameter_lock = result.value; lock_known = true;}
        refresh_baseline();
        next_config();
        break;
    case OP_LOCK:
        lock_pending = need_config = false;
        parameter_lock = result.value; lock_known = true;
        observation.configuration_ready = readiness_allowed();
        break;
    case OP_WATCHDOG:
        ctrl2 = result.value; ctrl2_known = true;
        if (!(ctrl2 & MP2724_WATCHDOG_MASK)) {watchdog_running = false; service_due = false;}
        else service_due = true;
        break;
    case OP_SERVICE:
        if (result.state == I2C_RESULT_COMMAND_ACCEPTED) {ctrl2 = result.value;ctrl2_known = true;}
        service_ms = HAL_GetTick(); service_due = false;
        watchdog_running = true;
        if (result.state == I2C_RESULT_COMMAND_UNCERTAIN) {
            observation.communication_fault = true;
            observation.configuration_ready = false;
        }
        if (recovering_watchdog) {recovering_watchdog = false;interrupt_pending = true;}
        break;
    case OP_CHARGE:
        observation.admitted = charge_target;
        if (charge_target) {phase_known = precharge_seen = false;}
        ctrl4 = result.value; ctrl4_known = enable_known = true; refresh_baseline();
        observation.paused = !(ctrl4 & MP2724_EN_CHG_MASK) && observation.warm;
        break;
    case OP_SHIP_DELAY: release_idle(); shipping_stage = 2U; break;
    case OP_SHIP: break; /* Outcome recorded before generic cancellation. */
    case OP_INPUT_AUDIT:
        config_value = expected_config(1U, result.value);
        if (result.value == config_value) {
            input_audit_due = false;
            observation.configuration_ready = readiness_allowed();
            release_idle();
        }
        else {observation.configuration_ready = false;input_write_due = true;}
        break;
    case OP_INPUT_CONFIG:
        input_audit_due = false;
        observation.configuration_ready = readiness_allowed();
        release_idle();
        break;
    case OP_NONE: break;
    }
}
void Charging_Init(const Charging_Profile *p)
{
    profile = p;
    memset(&observation, 0, sizeof observation);
    observation.profile_valid = valid_profile(p);
    operation = OP_NONE;
    config_index = poll_index = shipping_stage = 0U;
    config_mask = config_value = ctrl2 = ctrl4 = 0U;
    ctrl2_known = ctrl4_known = lock_pending = false;
    enable_known = ipre_known = lock_known = charge_target = false;
    baseline_required = true; ipre = parameter_lock = ipre_target = 0U;
    arm_stage = ARM_IDLE; battery_eligible = normal_operation = fault_inhibited = false;
    precharge_seen = phase_known = false; battery_sequence = precharge_sequence = phase_ms = 0U;
    charge_required = sleep_requested = shipping_requested = false;
    watchdog_running = service_due = recovering_watchdog = false;
    poll_active = cancel_requested = available_before = false;
    interrupt_pending = false;
    poll_started_ms = service_ms = HAL_GetTick();
    input_audit_due = config_write_due = input_write_due = false;
    validation = initial_poll = true;
    need_config = observation.profile_valid;
}
void Charging_BeginValidation(void)
{
    validation = true;
    baseline_required = true; charge_target = false;
    ipre_target = 0U; arm_stage = ARM_IDLE; precharge_seen = phase_known = false; observation.admitted = false;
    enable_known = ipre_known = lock_known = false; refresh_baseline();
    ctrl2_known = false;
    config_write_due = input_write_due = false;
    input_audit_due = false;
    sleep_requested = false;
    observation.sleep_ready = observation.configuration_ready = observation.status_ready = false;
    observation.ntc_fresh = false;
    need_config = observation.profile_valid;
    config_index = 0U;
    lock_pending = false;
    initial_poll = true;
    cancel_operation();
}
void Charging_EndValidation(void) {validation = false;}
static void revoke_admission(void)
{
    bool work = charge_target || observation.admitted || arm_stage != ARM_IDLE || ipre_target != 0U;
    charge_target = false; observation.admitted = false;
    arm_stage = ARM_IDLE; ipre_target = 0U; precharge_seen = phase_known = false;
    if (work) {
        baseline_required = true;
        observation.safe_baseline_ready = false;
        cancel_operation();
    }
}
void Charging_SetChargeRequired(bool required)
{
    charge_required = required;
    if (!required) revoke_admission();
}
void Charging_SetEligibility(bool eligible, uint32_t sequence, bool normal, bool fault)
{
    battery_eligible = eligible; battery_sequence = sequence;
    normal_operation = normal; fault_inhibited = fault;
    if (!eligible || fault || (normal && (arm_stage != ARM_IDLE || ipre_target != 0U)))
        revoke_admission();
}
void Charging_OnInterrupt(void) {interrupt_pending = true;}
void Charging_RequestPrepareSleep(void)
{
    sleep_requested = true;
    revoke_admission();
    baseline_required = true; observation.safe_baseline_ready = false;
    ctrl2_known = false; /* Reconcile possible writes before verifying disable. */
    config_write_due = input_write_due = false;
    input_audit_due = false;
    observation.configuration_ready = false;
    observation.sleep_ready = false;
    cancel_operation();
}
bool Charging_RequestShipping(void)
{
    if (shipping_requested) return false;
    shipping_requested = true;
    observation.shipping_requested = true;
    shipping_stage = 0U;
    Charging_RequestPrepareSleep();
    return true;
}
Charging_Observation Charging_GetObservation(void)
{
    Charging_Observation copy = observation;
    bool fresh = phase_known && HAL_GetTick() - phase_ms <= CHARGING_STATUS_MAX_AGE_MS;
    copy.active_charging = copy.active_charging && copy.admitted && fresh &&
        arm_stage == ARM_IDLE && enable_known && (ctrl4 & MP2724_EN_CHG_MASK);
    copy.topoff_active = copy.topoff_active && copy.admitted && fresh &&
        arm_stage == ARM_IDLE && enable_known && (ctrl4 & MP2724_EN_CHG_MASK);
    if (!copy.status_ready || HAL_GetTick() - copy.status_ms > CHARGING_STATUS_MAX_AGE_MS) {
        copy.ntc_fresh = copy.cool = copy.warm = false;
    }
    return copy;
}
void Charging_Update(bool available)
{
    uint32_t now = HAL_GetTick();
    if (!available) {
        observation.ntc_fresh = observation.status_ready = observation.configuration_ready = false;
        observation.cool = observation.warm = false;
        observation.sleep_ready = false;
        config_write_due = input_write_due = false;
        ctrl2_known = ctrl4_known = enable_known = ipre_known = lock_known = false;
        baseline_required = true; charge_target = false;
        ipre_target = 0U; arm_stage = ARM_IDLE; precharge_seen = phase_known = false; observation.admitted = false; refresh_baseline();
        input_audit_due = false;
        if (!cancel_requested) cancel_operation();
    } else if (!available_before) {
        input_audit_due = false;
        initial_poll = true;
        need_config = observation.profile_valid;
        config_index = 0U;
        lock_pending = false;
    }
    available_before = available;
    MP2724_Update();
    I2C_DeviceResult result = MP2724_GetResult();
    bool completed_round = operation == OP_CTRL2 && result.state == I2C_RESULT_READ && !result.recovering;
    observation.recovering = result.recovering;
    consume_result(result);
    if (!available || result.recovering || result.state == I2C_RESULT_PENDING || cancel_requested) return;
    if ((!desired_charge() || sleep_requested) && (charge_target || observation.admitted || arm_stage != ARM_IDLE))
        revoke_admission();
    if (cancel_requested) return;
    if (operation == OP_NONE && baseline_required && !observation.communication_fault) {
        if (!enable_known || (ctrl4 & MP2724_EN_CHG_MASK)) {
            configure(MP2724_REG_CHG_CTRL4, MP2724_EN_CHG_MASK, 0U, OP_INHIBIT); return;
        }
        if (!ipre_known || (ipre & MP2724_IPRE_MASK)) {
            observation.safe_baseline_ready = false;
            configure(MP2724_REG_CHG_PARAMETER1, MP2724_IPRE_MASK, 0U, OP_ZERO); return;
        }
        configure(MP2724_REG_CHG_CTRL0, MP2724_LOCK_CHG_MASK, MP2724_LOCK_CHG_MASK, OP_BASELINE_LOCK);return;
    }
    /* Audit/read and verified write are separate stages sharing the lease. */
    if (config_write_due) {
        uint8_t reg = mp2724_registers[config_index].address;
        if (sleep_requested || observation.communication_fault) {config_write_due = false;release_idle();}
        else if (configure(reg, config_mask, config_value, OP_CONFIG)) {config_write_due = false;return;}
        else {config_write_due = false;release_idle();}
    }
    if (input_write_due) {
        if (sleep_requested || observation.communication_fault) {input_write_due = false;release_idle();}
        else if (configure(MP2724_REG_IIN, stable_mask(1U), config_value, OP_INPUT_CONFIG)) {input_write_due = false;return;}
        else {input_write_due = false;release_idle();}
    }
    if (operation != OP_NONE) return;
    if (sleep_requested) {
        if (!ctrl2_known) {read_register(MP2724_REG_CHG_CTRL2, OP_WATCHDOG);return;}
        if (ctrl2 & MP2724_WATCHDOG_MASK) {
            configure(MP2724_REG_CHG_CTRL2, MP2724_WATCHDOG_MASK, 0U, OP_WATCHDOG);return;
        }
        observation.sleep_ready = false;
        if (shipping_requested && shipping_stage < 3U) {
            if (shipping_stage < 2U) configure(MP2724_REG_CHG_CTRL3, MP2724_BATTFET_DLY_MASK, 0U, OP_SHIP_DELAY);
            else if (command(MP2724_REG_CHG_CTRL3, MP2724_BATTFET_DIS_MASK, OP_SHIP)) shipping_stage = 3U;
        }
        I2C_DeviceResult current = MP2724_GetResult();
        observation.sleep_ready = operation == OP_NONE && current.state == I2C_RESULT_IDLE &&
            !current.recovering && !observation.idle_lease && !cancel_requested &&
            (!shipping_requested || shipping_stage == 3U);
        return;
    }
    /* A decoded departure reduces current before the next status transaction. */
    bool phase_fresh = phase_known && now - phase_ms <= CHARGING_STATUS_MAX_AGE_MS;
    bool arming = arm_stage != ARM_IDLE;
    bool precharge_allowed = desired_charge() && !observation.completed && !normal_operation && phase_fresh &&
        (observation.phase == MP2724_CHG_STAT_PRECHARGE ||
         (arming && observation.phase == MP2724_CHG_STAT_NOT_CHARGING));
    if (ipre_target != 0U && !precharge_allowed) {
        ipre_target = 0U;
        if (arming) {revoke_admission(); return;}
        configure(MP2724_REG_CHG_PARAMETER1, MP2724_IPRE_MASK, 0U, OP_ZERO); return;
    }
    if (arm_stage != ARM_IDLE) {
        if (!precharge_allowed) {revoke_admission();return;}
        switch (arm_stage) {
        case ARM_INHIBIT:
            configure(MP2724_REG_CHG_CTRL4, MP2724_EN_CHG_MASK, 0U, OP_ARM_INHIBIT); break;
        case ARM_UNLOCK:
            configure(MP2724_REG_CHG_CTRL0, MP2724_LOCK_CHG_MASK, 0U, OP_ARM_UNLOCK); break;
        case ARM_CURRENT:
            ipre_target = profile->registers[3] & MP2724_IPRE_MASK;
            configure(MP2724_REG_CHG_PARAMETER1, MP2724_IPRE_MASK, ipre_target, OP_ARM_CURRENT); break;
        case ARM_LOCK:
            configure(MP2724_REG_CHG_CTRL0, MP2724_LOCK_CHG_MASK, MP2724_LOCK_CHG_MASK, OP_ARM_LOCK); break;
        case ARM_ENABLE:
            configure(MP2724_REG_CHG_CTRL4, MP2724_EN_CHG_MASK, MP2724_EN_CHG_MASK, OP_ARM_ENABLE); break;
        case ARM_IDLE: break;
        }
        return;
    }
    if (observation.admitted && precharge_allowed && precharge_seen && ipre_target == 0U &&
        (int32_t)(battery_sequence - precharge_sequence) > 0) {
        arm_stage = ARM_INHIBIT;
        configure(MP2724_REG_CHG_CTRL4, MP2724_EN_CHG_MASK, 0U, OP_ARM_INHIBIT);return;
    }
    /* USB-dependent service outranks refresh flood; each complete status round
     * then yields to service/configuration rather than restarting on every IRQ. */
    if (!poll_active && ctrl2_known) {
        uint8_t target = observation.input_valid ? MP2724_WATCHDOG_40_S << MP2724_WATCHDOG_SHIFT : 0U;
        if ((ctrl2 & MP2724_WATCHDOG_MASK) != target) {
            configure(MP2724_REG_CHG_CTRL2, MP2724_WATCHDOG_MASK, target, OP_WATCHDOG);return;
        }
        if (observation.input_valid && (service_due || !watchdog_running || now - service_ms >= CHARGER_WATCHDOG_SERVICE_INTERVAL_MS)) {
            command(MP2724_REG_CHG_CTRL2, MP2724_WATCHDOG_RST_MASK, OP_SERVICE);return;
        }
    }
    if (!poll_active && (initial_poll || now - poll_started_ms >= CHARGING_POLL_INTERVAL_MS || (interrupt_pending && !completed_round))) {
        poll_active = true;
        poll_index = 0U;
        poll_started_ms = now;
        interrupt_pending = false;
    }
    if (poll_active) {
        if (poll_index < sizeof status_order) read_register(MP2724_REG_STATUS0 + status_order[poll_index], OP_STATUS);
        else if (poll_index == sizeof status_order) read_register(MP2724_REG_CHG_CTRL4, OP_CTRL4);
        else if (poll_index == sizeof status_order + 1U) read_register(MP2724_REG_CHG_CTRL3, OP_CTRL3);
        else read_register(MP2724_REG_CHG_CTRL2, OP_CTRL2);
        return;
    }
    if (observation.communication_fault || observation.watchdog_fault || observation.charger_fault || observation.hot || observation.cold) return;
    if (ctrl4_known && !(ctrl4 & MP2724_EN_CHG_MASK) && desired_charge() && !observation.completed) {
        charge_target = true;
        configure(MP2724_REG_CHG_CTRL4, MP2724_EN_CHG_MASK, MP2724_EN_CHG_MASK, OP_CHARGE);return;
    }
    if (input_audit_due && observation.profile_valid && observation.input_ready) {
        if (acquire_idle(MP2724_REG_IIN)) {
            if (!read_register(MP2724_REG_IIN, OP_INPUT_AUDIT)) release_idle();
        }
        else observation.configuration_ready = false;
        return;
    }
    if (!need_config || !observation.profile_valid || !observation.status_ready) return;
    if (lock_pending) {
        configure(MP2724_REG_CHG_CTRL0, MP2724_LOCK_CHG_MASK, MP2724_LOCK_CHG_MASK, OP_LOCK);return;
    }
    uint8_t reg = mp2724_registers[config_index].address;
    if (reg == MP2724_REG_IIN) {
        /* Battery-only configuration still requires a whole-register idle
         * lease; VIN_RDY qualifies a present input, not the lease itself. */
        if ((observation.input_valid && !observation.input_ready) || !acquire_idle(reg)) return;
    }
    if (!read_register(reg, OP_AUDIT)) release_idle();
}
