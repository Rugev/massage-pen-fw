#include "vibration.h"
#include "drv2624.h"
#include "fake_hal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Hardware boundary only: the real register driver and transaction engine run.
 * Test motor bytes are deliberately synthetic, never a board profile. */
static I2C_HandleTypeDef handle;
static uint8_t regs[256], *buffer, reg;
static bool pending, writing, drain_ready, available, enabled;
static uint8_t level;
static unsigned calibrations, failed_calibrations, writes, status_reads;
static uint32_t calibration_start, last_status_read_ms;
static unsigned auto_writes_while_running;
static bool cal_hangs, mismatch, uncertain_delivered, ignore_stop;
static int uncertain_go;
static uint8_t mismatch_reg;
static uint8_t events_reg[512], events_write[512];
static unsigned events;
static bool read_it(I2C_HandleTypeDef *h, uint16_t a, uint8_t r, uint8_t *b)
{
    assert(h == &handle && a == 0xb4 && available && !pending);
    pending = true; writing = false; reg = r; buffer = b; return true;
}
static bool write_it(I2C_HandleTypeDef *h, uint16_t a, uint8_t r, uint8_t *b)
{
    assert(h == &handle && a == 0xb4 && available && !pending);
    pending = true; writing = true; reg = r; buffer = b; return true;
}
static bool stop(I2C_HandleTypeDef *h)
{
    assert(h == &handle);
    if (!drain_ready) return false;
    pending = false; buffer = NULL; return true;
}
static const I2C_DeviceOps ops = {read_it, write_it, stop};
static Vibration_Profile profile(void)
{
    return (Vibration_Profile){ .validated = true,
        .mode = 0x08, .control = 0x80, .feedback_control = 0x52,
        .rated_voltage = 0x33, .od_clamp = 0x44,
        .lra_drive_control = 0x07, .bemf_timing = 0x22,
        .timing_control = 0x0c, .auto_cal_time = 0,
        .calibration_duration_ms = 250, .calibration_timeout_ms = 400,
        .rtp = {0, 31, 63, 127} };
}
static void reset(const Vibration_Profile *p)
{
    FakeHAL_Reset(); memset(regs, 0, sizeof regs);
    pending = false; drain_ready = true; available = true; enabled = true; level = 0;
    calibrations = failed_calibrations = writes = status_reads = events = 0;
    last_status_read_ms = 0;
    auto_writes_while_running = 0; cal_hangs = mismatch = false; uncertain_go = -1; uncertain_delivered = true; ignore_stop = false; mismatch_reg = 0x1f;
    DRV2624_Init(&handle, &ops); Vibration_Init(p);
}
static void motor_tick(void)
{
    if (regs[12] && (regs[7] & 3) == 3 && !cal_hangs &&
        (uint32_t)(HAL_GetTick() - calibration_start) >= 250) {
        regs[12] = 0; regs[1] |= 8;
        if (calibrations <= failed_calibrations) regs[1] |= 0x80;
        else {regs[0x21] = 0xa5; regs[0x22] = 0x5a; regs[0x23] = (regs[0x23] & 0xfc) | 3;}
    }
}
static void complete(void)
{
    assert(pending && available); assert(events < 512);
    events_reg[events] = reg; events_write[events++] = writing;
    if (writing) {
        writes++;
        if (regs[12] && (reg == 0x21 || reg == 0x22 || reg == 0x23)) auto_writes_while_running++;
        if (!(reg == 12 && ((uncertain_go == (*buffer & 1) && !uncertain_delivered) || (ignore_stop && !(*buffer & 1))))) regs[reg] = *buffer;
        if (reg == 12 && (*buffer & 1) && (regs[7] & 3) == 3) {
            calibrations++; calibration_start = HAL_GetTick();
        }
        if (reg == 12 && uncertain_go == (*buffer & 1)) {
            uncertain_go = -1; DRV2624_OnError(&handle); return;
        }
        pending = false; DRV2624_OnWriteComplete(&handle);
    } else {
        *buffer = regs[reg];
        if (mismatch && reg == mismatch_reg) *buffer ^= 1;
        if (reg == 1) {status_reads++; last_status_read_ms = HAL_GetTick(); regs[1] &= 0x60;}
        pending = false; DRV2624_OnReadComplete(&handle);
    }
}
static void step(void)
{
    motor_tick(); Vibration_Update(available, enabled, level);
    if (pending && available && drain_ready) complete();
    FakeHAL_AdvanceTick(1);
}
static void run(unsigned ms) {while (ms--) step();}
static void ready_configuration(void)
{
    for (unsigned i=0; i<200 && !Vibration_GetObservation().configuration_ready; ++i) step();
    assert(Vibration_GetObservation().configuration_ready);
}
static void ready_output(void)
{
    for (unsigned i=0; i<1000 && !Vibration_GetObservation().output_active; ++i) step();
    assert(Vibration_GetObservation().output_active);
}
static void no_profile_no_configuration(void)
{
    reset(NULL); level = 1; run(500);
    assert(!Vibration_GetObservation().profile_valid);
    assert(!Vibration_GetObservation().configuration_ready && !writes && !calibrations);
    Vibration_Profile p=profile(); p.validated=false; reset(&p); level=1; run(500); assert(!writes);
    p=profile(); p.calibration_duration_ms=0; reset(&p); level=1; run(500); assert(!writes);
    p=profile(); p.control=0; reset(&p); level=1; run(500); assert(!writes);
    p=profile(); p.mode=4; reset(&p); level=1; run(500); assert(!writes);
}
/* Missing initial error check would write settings before the fault is seen. */
static void errors_precede_configuration(void)
{
    Vibration_Profile p=profile(); reset(&p); regs[1]=4; run(100);
    assert(Vibration_GetObservation().driver_fault && !writes);
    assert(DRV2624_GetStatus().value & 4);
    assert(Vibration_GetObservation().status.value & 4);
}
static void zero_and_verified_configuration(void)
{
    Vibration_Profile p=profile(); reset(&p); ready_configuration(); run(300);
    assert(!calibrations && !Vibration_GetObservation().calibration_ready);
    assert(regs[8]==0x80 && regs[0x1f]==0x33 && regs[0x20]==0x44 && regs[0x23]==0x52);
    assert(events_reg[0]==1 && !events_write[0]);
    assert(!auto_writes_while_running);
    reset(&p); mismatch=true; run(300);
    assert(Vibration_GetObservation().communication_fault && !Vibration_GetObservation().configuration_ready);
}
static void calibration_retained_and_constant_rtp(void)
{
    Vibration_Profile p=profile(); reset(&p); ready_configuration(); level=1;
    run(100); assert(!Vibration_GetObservation().calibration_ready && !Vibration_GetObservation().output_active);
    ready_output(); Vibration_Observation o=Vibration_GetObservation();
    assert(calibrations==1 && o.calibration_ready && o.calibration_retained);
    assert(o.a_cal_comp==0xa5 && o.a_cal_bemf==0x5a && o.bemf_gain==3);
    assert(regs[14]==31 && (regs[7]&3)==0 && regs[12]==1);
    level=3; run(30); assert(regs[14]==127 && Vibration_GetObservation().applied_level==3);
    assert(calibrations==1 && !auto_writes_while_running);
    level=0; run(30); assert(!Vibration_GetObservation().output_active && !regs[12]);
    level=2; ready_output(); assert(regs[14]==63 && calibrations==1);
}
static void three_attempts_and_fresh_status(void)
{
    Vibration_Profile p=profile(); reset(&p); failed_calibrations=3; level=1; run(1800);
    assert(calibrations==3 && Vibration_GetObservation().calibration_fault);
    assert(!Vibration_GetObservation().calibration_retained && !Vibration_GetObservation().output_active);
    reset(&p); failed_calibrations=1; level=1; ready_output();
    assert(calibrations==2 && Vibration_GetObservation().calibration_ready);
    assert(DRV2624_GetStatus().value & 0x80); /* Old failure remains visible to app. */
    assert(DRV2624_GetStatus().value & 8);
    reset(&p); regs[1]=8; cal_hangs=true; level=1; run(1600);
    assert(calibrations==3 && Vibration_GetObservation().calibration_fault);
    assert(!Vibration_GetObservation().calibration_retained); /* old DONE cannot complete new attempt */
}
static void power_sleep_and_reboot(void)
{
    Vibration_Profile p=profile(); reset(&p); level=1; ready_output();
    available=false; run(30); assert(!Vibration_GetObservation().calibration_ready);
    assert(Vibration_GetObservation().calibration_retained);
    memset(regs,0,sizeof regs); available=true; ready_output();
    assert(calibrations==1 && regs[0x21]==0xa5 && regs[0x22]==0x5a && (regs[0x23]&3)==3);
    enabled=false; run(30); assert(!Vibration_GetObservation().output_active && !regs[12]);
    enabled=true; ready_output(); assert(calibrations==1);
    enabled=false; run(30); DRV2624_Init(&handle,&ops); Vibration_Init(&p); enabled=true;
    assert(!Vibration_GetObservation().calibration_retained); ready_output(); assert(calibrations==2);
}
static void cancellation_and_uncertain_commands(void)
{
    Vibration_Profile p=profile(); reset(&p); level=1;
    while (!calibrations) { step(); }
    level=0; run(400);
    assert(!Vibration_GetObservation().calibration_retained && !Vibration_GetObservation().output_active && !regs[12]);
    level=1; ready_output(); assert(calibrations==2 && !auto_writes_while_running);
    reset(&p); level=1; uncertain_go=1; ready_output();
    assert(calibrations==1 && Vibration_GetObservation().calibration_retained);
    reset(&p); level=1; while(!calibrations) step();
    drain_ready=false; level=0; run(10);
    assert(!Vibration_GetObservation().calibration_retained && !Vibration_GetObservation().output_active);
    assert(auto_writes_while_running==0);
    drain_ready=true; uncertain_go=0; run(60);
    assert(!regs[12] && !Vibration_GetObservation().calibration_retained && !auto_writes_while_running);
}
/* An uncertain command's write buffer is not a GO observation. */
static void uncertain_rtp_requires_running_observation(void)
{
    Vibration_Profile p=profile(); reset(&p); level=1;
    while (!Vibration_GetObservation().calibration_retained) {step();}
    uncertain_go=1; uncertain_delivered=false; run(100);
    assert(!Vibration_GetObservation().output_active);
    assert(Vibration_GetObservation().communication_fault);
}
/* Power loss while already stopping must cancel the stop transfer too. */
static void power_loss_during_stop(void)
{
    Vibration_Profile p=profile(); reset(&p); level=1; ready_output(); level=0;
    for (unsigned i=0; i<100; ++i) {
        Vibration_Update(available,enabled,level);
        if (pending && writing && reg==12) break;
        if (pending) complete();
        FakeHAL_AdvanceTick(1);
    }
    assert(pending && writing && reg==12);
    available=false; run(30); assert(!Vibration_GetObservation().configuration_ready);
    assert(Vibration_GetObservation().calibration_retained);
    memset(regs,0,sizeof regs); available=true; level=1; ready_output();
    assert(calibrations==1 && !auto_writes_while_running);
}
/* GO=0 during automatic braking is too early to restart or claim standby. */
static void standby_braking_drains_before_restart(void)
{
    Vibration_Profile p=profile(); p.control=0x88; reset(&p); level=1; ready_output();
    level=0; run(10); level=1; run(30);
    assert(!regs[12] && !Vibration_GetObservation().output_active);
    ready_output(); assert(calibrations==1);
}
static void restoration_readback_failure_inhibits(void)
{
    Vibration_Profile p=profile(); reset(&p); level=1; ready_output();
    available=false; run(30); memset(regs,0,sizeof regs);
    available=true; mismatch=true; mismatch_reg=0x21; run(300);
    assert(Vibration_GetObservation().communication_fault);
    assert(Vibration_GetObservation().calibration_retained);
    assert(!Vibration_GetObservation().calibration_ready && !Vibration_GetObservation().output_active);
    assert(calibrations==1);
}
/* No partial RAM triplet may escape if cancelled before the final capture. */
static void cancellation_during_result_capture(void)
{
    Vibration_Profile p=profile(); reset(&p); level=1;
    bool capture=false;
    for (unsigned i=0; i<1000; ++i) {
        motor_tick(); Vibration_Update(available,enabled,level);
        if (pending && !writing && reg==0x23 && calibrations && !regs[12]) {capture=true; break;}
        if (pending) complete();
        FakeHAL_AdvanceTick(1);
    }
    assert(capture); level=0; run(100);
    assert(!Vibration_GetObservation().calibration_retained && !Vibration_GetObservation().calibration_ready);
    assert(!Vibration_GetObservation().output_active && !auto_writes_while_running);
}
static void acknowledged_status_keeps_module_fault(void)
{
    Vibration_Profile p=profile(); reset(&p); level=1; ready_output();
    regs[1]=2; run(120); assert(Vibration_GetObservation().driver_fault);
    DRV2624_StatusSnapshot status=DRV2624_GetStatus();
    assert(status.value & 2); assert(DRV2624_AcknowledgeStatus(status.sequence));
    run(10); assert(Vibration_GetObservation().driver_fault && !Vibration_GetObservation().output_active);
}
static void calibration_polls_status_at_cadence(void)
{
    Vibration_Profile p=profile(); reset(&p); level=1; cal_hangs=true;
    while (!calibrations) {step();}
    unsigned before=status_reads; run(130);
    assert(status_reads > before);
    assert(!Vibration_GetObservation().calibration_retained);
}
/* A physical-stop deadline may fault while a GO read is queued. App power-off
 * must not allow that queued operation to start after the fault publication. */
static void stop_fault_cancels_queued_transport(void)
{
    for (unsigned timeout=400; timeout<=402; ++timeout) {
        Vibration_Profile p=profile(); p.calibration_timeout_ms=timeout;
        reset(&p); level=1; ready_output(); ignore_stop=true; level=0;
        for (unsigned i=0; i<600 && !Vibration_GetObservation().communication_fault; ++i) step();
        assert(Vibration_GetObservation().communication_fault);
        available=false; run(30);
        assert(!Vibration_GetObservation().output_active);
    }
}
/* Continuous accepted level changes cannot outrank overdue protection reads.
 * Ten foreground cycles allow the in-flight verified RTP transaction and the
 * subsequent STATUS acquisition to drain beyond the 100 ms poll deadline. */
static void changing_levels_cannot_starve_protection_status(void)
{
    Vibration_Profile p=profile(); reset(&p); level=1; ready_output();
    unsigned before=status_reads;
    for (unsigned i=0; i<600; ++i) {
        level=Vibration_GetObservation().applied_level==1 ? 2 : 1;
        step();
        assert((uint32_t)(HAL_GetTick()-last_status_read_ms)<=110);
    }
    assert(status_reads>=before+5 && Vibration_GetObservation().output_active);
    regs[1]|=1;
    for (unsigned i=0; i<110 && !Vibration_GetObservation().driver_fault; ++i) {
        level=Vibration_GetObservation().applied_level==1 ? 2 : 1;
        step();
    }
    assert(Vibration_GetObservation().driver_fault);
    assert(!Vibration_GetObservation().output_active && !Vibration_GetObservation().calibration_ready);
    assert(DRV2624_GetStatus().value & 1);
    available=false; run(20);
    assert(Vibration_GetObservation().driver_fault && !Vibration_GetObservation().output_active);
}
int main(void)
{
    no_profile_no_configuration(); errors_precede_configuration(); zero_and_verified_configuration();
    calibration_retained_and_constant_rtp(); three_attempts_and_fresh_status();
    power_sleep_and_reboot(); cancellation_and_uncertain_commands();
    uncertain_rtp_requires_running_observation(); power_loss_during_stop();
    standby_braking_drains_before_restart();
    restoration_readback_failure_inhibits(); cancellation_during_result_capture();
    acknowledged_status_keeps_module_fault(); calibration_polls_status_at_cadence();
    stop_fault_cancels_queued_transport();
    changing_levels_cannot_starve_protection_status();
    puts("vibration: all tests passed"); return 0;
}
