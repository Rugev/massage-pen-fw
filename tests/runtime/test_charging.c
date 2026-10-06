#include "charging.h"
#include "mp2724.h"
#include "fake_hal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Real driver/engine, fake device boundary. Remaining profile choices below
 * are explicit synthetic test agreements, never a hardware board profile. */
static I2C_HandleTypeDef handle;
static uint8_t regs[256], reg, *buffer;
static bool pending, writing, drain_ready, available, allow_idle, leased;
static bool mismatch, uncertain_shipping, fail_reads;
static unsigned writes, shipping_writes, watchdog_resets, charge_toggles, unrelated_commands;
static unsigned status_reads[6], leases, releases;
static uint32_t last_errors;
static struct {uint8_t reg,value; bool write;} trace[30000];
static unsigned trace_count;
static bool read_it(I2C_HandleTypeDef *h,uint16_t a,uint8_t r,uint8_t *b)
{assert(h==&handle && a==(MP2724_I2C_ADDRESS_7BIT<<1) && available && !pending); pending=true;writing=false;reg=r;buffer=b;return true;}
static bool write_it(I2C_HandleTypeDef *h,uint16_t a,uint8_t r,uint8_t *b)
{assert(h==&handle && a==(MP2724_I2C_ADDRESS_7BIT<<1) && available && !pending); pending=true;writing=true;reg=r;buffer=b;return true;}
static bool stop(I2C_HandleTypeDef *h)
{assert(h==&handle);if(!drain_ready)return false;pending=false;buffer=NULL;return true;}
static bool acquire(uint8_t r)
{assert(!leased);if(!allow_idle)return false;if(r==1 && (regs[0x12]&0x40) && !(regs[0x12]&0x20))return false;leased=true;leases++;return true;}
static void release(uint8_t r)
{assert((r==1||r==8)&&leased&&!pending);leased=false;releases++;}
static const I2C_DeviceOps ops={read_it,write_it,stop};
static const Charging_IdleOps idle={acquire,release};
static Charging_Profile profile(void)
{return (Charging_Profile){.agreed=true,.registers={0x10,0,(MP2724_VPRE_3000_MV << MP2724_VPRE_SHIFT) | 0x23, ((CHARGER_PRECHARGE_OPERATING_CURRENT_MA / MP2724_IPRE_STEP_MA) << MP2724_IPRE_SHIFT) | 3U,0x06,0x18,0x04,0x1e,0x20,0x03,0x20,0x51,0x21,0x4e,0,0},.idle=&idle};}
static void reset(const Charging_Profile *p)
{
 trace_count=0;FakeHAL_Reset();memset(regs,0,sizeof regs);memset(status_reads,0,sizeof status_reads);
 pending=false;drain_ready=available=allow_idle=true;leased=false;mismatch=uncertain_shipping=fail_reads=false;
 writes=shipping_writes=watchdog_resets=charge_toggles=leases=releases=unrelated_commands=0;last_errors=0;
 regs[1]=31; regs[7]=0x10;regs[0x12]=0x60;regs[9]=3;regs[0x10]=0xc0;
 MP2724_Init(&handle,&ops);Charging_Init(p);Charging_SetChargeRequired(true);Charging_SetEligibility(true,1U,false,false);
}
static void complete(void)
{
 assert(pending);
 assert(trace_count<30000);trace[trace_count].reg=reg;trace[trace_count].value=writing?*buffer:regs[reg];trace[trace_count++].write=writing;
 if(writing){
  if(reg==1||reg==8) assert(leased || (reg==8 && (*buffer&0x80)));
  if (reg==3 && (*buffer&MP2724_IPRE_MASK) > (regs[3]&MP2724_IPRE_MASK))
   assert(!(regs[0]&MP2724_LOCK_CHG_MASK) && !(regs[9]&MP2724_EN_CHG_MASK));
  writes++;if(reg==9 && ((regs[9]^*buffer)&1))charge_toggles++;
  if(reg==8 && (*buffer&0x80))shipping_writes++;
  if(reg==7 && (*buffer&0x40)){watchdog_resets++;regs[0x12]&=~2U;}
  if((reg==0&&(*buffer&0x80))||(reg==0x0a&&(*buffer&0x10)))unrelated_commands++;
  regs[reg]=*buffer;if(reg==7)regs[7]&=~0x40U;if(reg==0)regs[0]&=~0x80U;if(reg==0x0a)regs[0x0a]&=~0x10U;
  if(reg==8 && (*buffer&0x80) && uncertain_shipping){uncertain_shipping=false;MP2724_OnError(&handle);return;}
  pending=false;MP2724_OnWriteComplete(&handle);
 }else{
  if(fail_reads){MP2724_OnError(&handle);return;}
  *buffer=regs[reg];if(mismatch&&reg==0x0e)*buffer^=1;
  if(reg>=0x11&&reg<=0x16){status_reads[reg-0x11]++;if(reg==0x13)last_errors=HAL_GetTick();}
  pending=false;MP2724_OnReadComplete(&handle);
 }
}
static void step(void)
{Charging_Update(available);if(pending&&available&&drain_ready)complete();FakeHAL_AdvanceTick(1);}
static void run(unsigned ms){while(ms--)step();}
static void ready(void)
{for(unsigned i=0;i<600&&!Charging_GetObservation().configuration_ready;i++)step();assert(Charging_GetObservation().configuration_ready);Charging_EndValidation();}
/* A null/incomplete profile cannot be promoted from readable status. */
static void monitoring_without_profile(void)
{
 reset(NULL);run(200);Charging_Observation o=Charging_GetObservation();
 assert(!o.configuration_ready&&!o.profile_valid&&o.status_ready&&o.input_valid&&o.input_ready);
 assert(!o.active_charging);regs[0x12]=0x40;regs[0x13]=0x60;Charging_OnInterrupt();run(30);
 o=Charging_GetObservation();assert(o.input_valid&&!o.input_ready&&!o.active_charging);
 regs[0x13]|=2;Charging_OnInterrupt();run(30);assert(Charging_GetObservation().charger_fault);
 Charging_Profile p=profile();p.agreed=false;reset(&p);run(200);assert(!Charging_GetObservation().configuration_ready);
 p=profile();p.registers[9]|=4;reset(&p);run(200);assert(!Charging_GetObservation().profile_valid);
}
/* Completion/readback, reserved bits and whole-register ownership all matter. */
static void verified_configuration_and_idle_lease(void)
{
 Charging_Profile p=profile();reset(&p);allow_idle=false;run(300);assert(!Charging_GetObservation().configuration_ready);
 assert(Charging_GetObservation().status_ready&&status_reads[2]>=3);
 allow_idle=true;ready();assert(regs[1]==14&&regs[0x0e]==0x4e&&regs[0x10]==0xc0&&regs[0]&0x10);
 assert(leases==releases&&!leased);
 reset(&p);regs[8]=0xa0;run(400);assert(Charging_GetObservation().charger_fault&&!Charging_GetObservation().configuration_ready);
 reset(&p);mismatch=true;run(400);assert(Charging_GetObservation().communication_fault&&!Charging_GetObservation().configuration_ready);
 reset(&p);regs[0x12]=0x40;run(250);assert(!Charging_GetObservation().configuration_ready&&regs[1]==31);
 regs[0x12]=0x60;Charging_OnInterrupt();ready();assert(regs[1]==14);
}
/* Reading STATUS1 alone cannot hide subsequent fault/NTC reads under IRQ flood. */
static void polling_interrupts_and_errors(void)
{
 Charging_Profile p=profile();reset(&p);ready();unsigned before=status_reads[2];run(110);assert(status_reads[2]>before);
 regs[0x13]=1;for(unsigned i=0;i<40;i++){Charging_OnInterrupt();step();}
 assert(Charging_GetObservation().charger_fault);
 for(unsigned i=0;i<400;i++){Charging_OnInterrupt();step();assert(HAL_GetTick()-last_errors<110);}
 assert(status_reads[3]>10);
 reset(&p);ready();regs[8]|=0x80;run(110);assert(Charging_GetObservation().charger_fault&&!Charging_GetObservation().configuration_ready);
}
/* Startup/wake may restore thresholds; ordinary runtime expiry must latch. */
static void watchdog_recovery_and_runtime_fault(void)
{
 Charging_Profile p=profile();reset(&p);regs[0x12]|=2;ready();assert(!Charging_GetObservation().watchdog_fault&&watchdog_resets);
 regs[0x12]|=2;Charging_OnInterrupt();run(50);assert(Charging_GetObservation().watchdog_fault&&!Charging_GetObservation().configuration_ready);
 reset(&p);ready();Charging_RequestPrepareSleep();run(30);assert(Charging_GetObservation().sleep_ready);
 regs[0x0e]=0x99;regs[0x12]|=2;Charging_BeginValidation();ready();assert(regs[0x0e]==0x4e&&!Charging_GetObservation().watchdog_fault);
}
/* Warm pause resumes once, but done must not be restarted by a warm/cool cycle. */
static void warm_completion_and_usb_watchdog(void)
{
 Charging_Profile p=profile();reset(&p);regs[0x13]=0x60;ready();
 regs[0x14]=8;Charging_OnInterrupt();run(40);assert(!(regs[9]&1)&&Charging_GetObservation().paused);
 unsigned service=watchdog_resets;run(10050);assert(watchdog_resets>service&&(regs[7]&0x30)==0x10);
 regs[0x14]=0;Charging_OnInterrupt();run(40);assert(regs[9]&1);
 regs[0x13]=0xa0;Charging_OnInterrupt();run(40);unsigned toggles=charge_toggles;
 regs[0x14]=8;Charging_OnInterrupt();run(40);regs[0x14]=0;Charging_OnInterrupt();run(40);
 assert(charge_toggles==toggles+1&&!(regs[9]&1)&&Charging_GetObservation().completed);
 service=watchdog_resets;run(10050);assert(watchdog_resets>service);
 regs[0x12]=0;Charging_OnInterrupt();run(40);assert(!(regs[7]&0x30)&&!Charging_GetObservation().ntc_fresh);
 Charging_RequestPrepareSleep();run(40);assert(Charging_GetObservation().sleep_ready);
 service=watchdog_resets;run(10050);assert(watchdog_resets==service);
}
/* Shipping is deliberate, immediate and never retries an uncertain write. */
static void shipping_commands_and_drain(void)
{
 Charging_Profile p=profile();reset(&p);ready();regs[0]=0x90;regs[7]|=0x40;regs[8]|=0x40;regs[0x0a]|=0x10;
 uncertain_shipping=true;assert(Charging_RequestShipping());run(100);
 assert(shipping_writes==1&&!(regs[8]&0x40)&&Charging_GetObservation().shipping_uncertain);
 assert(!unrelated_commands);run(100);assert(shipping_writes==1);
 assert(!Charging_RequestShipping());
 reset(&p);ready();assert(Charging_RequestShipping());run(100);assert(Charging_GetObservation().shipping_accepted&&shipping_writes==1);
 reset(&p);for(unsigned i=0;i<600;i++){Charging_Update(available);if(pending&&writing&&reg==1)break;if(pending)complete();
  FakeHAL_AdvanceTick(1);}assert(pending&&writing&&reg==1);drain_ready=false;Charging_RequestPrepareSleep();run(30);
 assert(leased&&Charging_GetObservation().idle_lease&&!Charging_GetObservation().sleep_ready);
 drain_ready=true;run(40);assert(!leased&&leases==releases);
}
/* Status failures invalidate freshness, while observed hot/cold/error stay latched. */
static void stale_ntc_and_retained_faults(void)
{
 reset(NULL);regs[0x14]=0x20;run(60);assert(Charging_GetObservation().hot&&Charging_GetObservation().ntc_fresh);
 available=false;run(5);assert(!Charging_GetObservation().ntc_fresh&&Charging_GetObservation().hot);
 available=true;fail_reads=true;run(100);assert(Charging_GetObservation().communication_fault&&Charging_GetObservation().hot&&!Charging_GetObservation().ntc_fresh);
 reset(NULL);regs[0x12]=0;regs[0x14]=0x10;run(40);assert(!Charging_GetObservation().ntc_fresh&&!Charging_GetObservation().cool);
}
/* New CC/source detection must never raise the applied USB ceiling. IRQ flood
 * must also yield to thermal control and profile verification. */
static void source_changes_and_refresh_fairness(void)
{
 Charging_Profile p=profile();reset(&p);regs[1]=4;ready();assert(regs[1]==4);
 regs[1]=31;Charging_OnInterrupt();run(80);assert(regs[1]==14);
 allow_idle=false;run(150);assert(!Charging_GetObservation().configuration_ready);
 allow_idle=true;run(150);assert(Charging_GetObservation().configuration_ready);
 regs[0x13]=0x60;regs[0x14]=8;
 for(unsigned i=0;i<50;i++){Charging_OnInterrupt();step();}
 assert(!(regs[9]&1)&&Charging_GetObservation().paused);
 reset(&p);for(unsigned i=0;i<600;i++){Charging_OnInterrupt();step();}
 assert(Charging_GetObservation().configuration_ready);
}
/* Converter-off status and failed acquisitions cannot establish live NTC data. */
static void freshness_requires_powered_ntc(void)
{
 reset(NULL);regs[9]=0;regs[0x14]=0x10;run(50);
 assert(!Charging_GetObservation().ntc_fresh&&!Charging_GetObservation().cool);
 Charging_Profile p=profile();reset(&p);ready();Charging_RequestPrepareSleep();fail_reads=true;run(100);
 assert(Charging_GetObservation().communication_fault&&!Charging_GetObservation().sleep_ready);
 reset(NULL);assert(Charging_RequestShipping());run(100);
 assert(Charging_GetObservation().shipping_requested&&!Charging_GetObservation().shipping_accepted&&!shipping_writes);
}
/* Cancellation after a physical watchdog-enable write must reacquire ctrl2,
 * disable it, and verify that disable before publishing sleep readiness. */
static void sleep_cancellation_after_watchdog_write(void)
{
    reset(NULL);
    regs[7] = 0;
    for (unsigned i = 0; i < 100; ++i) {
        Charging_Update(available);
        if (pending && writing && reg == 7 && (*buffer & 0x30)) break;
        if (pending) complete();
        FakeHAL_AdvanceTick(1);
    }
    assert(pending && writing && reg == 7 && (*buffer & 0x30));
    complete(); /* Device enabled WDT; verification has not happened. */
    assert(regs[7] & 0x30);
    Charging_RequestPrepareSleep();
    for (unsigned i = 0; i < 40; ++i) {
        step();
        if (Charging_GetObservation().sleep_ready) assert(!(regs[7] & 0x30));
    }
    assert(Charging_GetObservation().sleep_ready && !(regs[7] & 0x30));
}
/* A shipping delay lease and its subsequent command both retain ownership;
 * sleep readiness must stay false throughout every pending transfer/drain. */
static void sleep_readiness_during_shipping_drain(void)
{
    Charging_Profile p = profile();
    reset(&p);
    ready();
    assert(Charging_RequestShipping());
    for (unsigned i = 0; i < 100; ++i) {
        Charging_Update(available);
        I2C_DeviceResult result = MP2724_GetResult();
        if (result.state == I2C_RESULT_PENDING || result.recovering || leased)
            assert(!Charging_GetObservation().sleep_ready);
        if (pending && reg == 8 && writing) break;
        if (pending) complete();
        FakeHAL_AdvanceTick(1);
    }
    assert(pending && reg == 8 && writing && leased);
    drain_ready = false;
    Charging_RequestPrepareSleep();
    run(10);
    assert(leased && Charging_GetObservation().recovering && !Charging_GetObservation().sleep_ready);
    drain_ready = true;
    run(100);
    assert(shipping_writes == 1 && Charging_GetObservation().shipping_accepted);
}
/* Cancelling an already-transmitted shipping command never permits replay,
 * including an uncertain terminal result whose buffer is still draining. */
static void shipping_cancellation_never_replays(void)
{
    for (unsigned mode = 0; mode < 3; ++mode) {
        Charging_Profile p = profile();
        reset(&p);
        ready();
        assert(Charging_RequestShipping());
        for (unsigned i = 0; i < 100; ++i) {
            Charging_Update(available);
            if (pending && writing && reg == 8 && (*buffer & 0x80)) break;
            if (pending) complete();
            FakeHAL_AdvanceTick(1);
        }
        assert(pending && writing && reg == 8 && (*buffer & 0x80));
        uncertain_shipping = mode == 2;
        complete();
        assert(shipping_writes == 1);
        if (mode == 2) {
            drain_ready = false;
            Charging_Update(available); /* Publish UNCERTAIN while retaining buffer. */
            assert(MP2724_GetResult().recovering);
        }
        if (mode == 0) available = false;
        else Charging_RequestPrepareSleep();
        for (unsigned i = 0; i < 5; ++i) {
            step();
            I2C_DeviceResult result = MP2724_GetResult();
            if (!available || result.state == I2C_RESULT_PENDING || result.recovering)
                assert(!Charging_GetObservation().sleep_ready);
        }
        if (mode == 2) {
            run(30);
            Charging_Observation o = Charging_GetObservation();
            assert(o.recovering && o.shipping_uncertain && o.communication_fault);
            assert(!o.sleep_ready && shipping_writes == 1);
        }
        available = drain_ready = true;
        run(100);
        assert(shipping_writes == 1);
        assert(Charging_GetObservation().shipping_uncertain);
        assert(!Charging_GetObservation().shipping_accepted);
    }
}
/* A battery-only IIN lease is supplied explicitly by the board adapter;
 * VIN_RDY and battery NTC freshness do not establish that ownership. */
static void battery_only_configuration_uses_explicit_idle_lease(void)
{
    Charging_Profile p = profile();
    reset(&p);
    regs[0x12] = 0U;
    allow_idle = false;
    run(300U);
    Charging_Observation o = Charging_GetObservation();
    assert(o.profile_valid && o.status_ready && !o.configuration_ready);
    assert(!o.input_valid && !o.input_ready && !o.ntc_fresh);
    assert(leases == 0U && regs[1] == 31U);
    allow_idle = true;
    ready();
    o = Charging_GetObservation();
    assert(o.configuration_ready && o.status_ready);
    assert(!o.input_valid && !o.input_ready && !o.ntc_fresh && !o.cool && !o.warm);
    assert(leases != 0U && leases == releases && !leased);
    assert(regs[1] == 14U && !(regs[7] & MP2724_WATCHDOG_MASK));
}
static void safe_baseline_without_profile_or_lease(void)
{
 for(unsigned mode=0;mode<3;mode++) {
  Charging_Profile p=profile();if(mode==1)p.agreed=false;
  reset(mode==0?NULL:&p);allow_idle=false;regs[3]=0xf3;
  for(unsigned i=0;i<200;i++) {
   step();
   if(pending && (reg==0 || reg==1)) assert(!(regs[9]&MP2724_EN_CHG_MASK) && !(regs[3]&MP2724_IPRE_MASK));
  }
  assert(!(regs[9]&MP2724_EN_CHG_MASK));
  assert(!(regs[3]&MP2724_IPRE_MASK));
  assert((regs[9]&MP2724_EN_BUCK_MASK) && (regs[3]&MP2724_ITERM_MASK)==3U);
  assert(!Charging_GetObservation().configuration_ready);
  assert(Charging_GetObservation().safe_baseline_ready);
 }
}
static void baseline_cancellation_reconciles_transmitted_write(void)
{
 reset(NULL);regs[3]=0xf3;
 for(unsigned i=0;i<100;i++) {
  Charging_Update(available);
  if(pending&&writing&&reg==3)break;
  if(pending)complete();
  FakeHAL_AdvanceTick(1);
 }
 assert(pending&&writing&&reg==3);complete();
 Charging_BeginValidation();
 assert(!Charging_GetObservation().safe_baseline_ready);
 run(100);assert(Charging_GetObservation().safe_baseline_ready);
 assert(!(regs[9]&1)&&!(regs[3]&MP2724_IPRE_MASK));
}
static void stale_input_completion_cannot_restore_validation_readiness(void)
{
 Charging_Profile p=profile();reset(&p);ready();Charging_OnInterrupt();
 for(unsigned i=0;i<200;i++) {
  Charging_Update(available);
  if(pending&&!writing&&reg==1&&leased)break;
  if(pending)complete();
  FakeHAL_AdvanceTick(1);
 }
 assert(pending&&!writing&&reg==1&&leased);
 complete();Charging_BeginValidation();allow_idle=false;run(150);
 assert(!Charging_GetObservation().configuration_ready);
 assert(Charging_GetObservation().safe_baseline_ready);
 assert(!leased&&leases==releases);
}
static void admitted_precharge_requires_new_adc(void)
{
 Charging_Profile p=profile();reset(&p);ready();run(40);
 assert((regs[9]&1) && !(regs[3]&MP2724_IPRE_MASK));
 regs[0x13]=MP2724_CHG_STAT_PRECHARGE<<MP2724_CHG_STAT_SHIFT;
 Charging_OnInterrupt();run(40);assert(!(regs[3]&MP2724_IPRE_MASK));
 unsigned arm_trace=trace_count;
 Charging_SetEligibility(true,2U,false,false);run(40);
 const uint8_t ordered_regs[]={9U,0U,3U,0U,9U};
 const uint8_t ordered_masks[]={MP2724_EN_CHG_MASK,MP2724_LOCK_CHG_MASK,MP2724_IPRE_MASK,MP2724_LOCK_CHG_MASK,MP2724_EN_CHG_MASK};
 const uint8_t ordered_values[]={0U,0U,p.registers[3]&MP2724_IPRE_MASK,MP2724_LOCK_CHG_MASK,MP2724_EN_CHG_MASK};
 unsigned stage=0U;bool awaiting_readback=false;
 for(unsigned i=arm_trace;i<trace_count && stage<5U;i++) {
  if(trace[i].write) {
   assert(!awaiting_readback && trace[i].reg==ordered_regs[stage]);
   assert((trace[i].value&ordered_masks[stage])==ordered_values[stage]);awaiting_readback=true;
  } else if(awaiting_readback && trace[i].reg==ordered_regs[stage]) {
   assert((trace[i].value&ordered_masks[stage])==ordered_values[stage]);awaiting_readback=false;stage++;
  }
 }
 assert(stage==5U);
 assert((regs[3]&MP2724_IPRE_MASK)==(p.registers[3]&MP2724_IPRE_MASK));
 assert((regs[0]&MP2724_LOCK_CHG_MASK) && (regs[9]&1));
 unsigned toggles=charge_toggles;run(120);assert(charge_toggles==toggles);
 regs[0x13]=MP2724_CHG_STAT_FAST<<MP2724_CHG_STAT_SHIFT;
 Charging_OnInterrupt();run(40);assert(!(regs[3]&MP2724_IPRE_MASK)&&charge_toggles==toggles);
 regs[0x13]=MP2724_CHG_STAT_PRECHARGE<<MP2724_CHG_STAT_SHIFT;
 Charging_OnInterrupt();run(40);assert(!(regs[3]&MP2724_IPRE_MASK));
 Charging_SetEligibility(true,3U,false,false);run(40);assert(regs[3]&MP2724_IPRE_MASK);
 Charging_SetEligibility(true,4U,true,false);run(50);assert(!(regs[3]&MP2724_IPRE_MASK));
}
static void nonprecharge_and_ineligible_never_raise_current(void)
{
 Charging_Profile p=profile();
 for(unsigned phase=0U;phase<=MP2724_CHG_STAT_DONE;phase++) {
  if(phase==MP2724_CHG_STAT_PRECHARGE)continue;
  reset(&p);regs[0x13]=phase<<MP2724_CHG_STAT_SHIFT;ready();
  Charging_SetEligibility(true,2U,false,false);run(120);assert(!(regs[3]&MP2724_IPRE_MASK));
 }
 for(unsigned mode=0U;mode<3U;mode++) {
  reset(&p);ready();run(40);regs[0x13]=MP2724_CHG_STAT_PRECHARGE<<MP2724_CHG_STAT_SHIFT;
  Charging_OnInterrupt();run(40);
  if(mode==0U)Charging_SetEligibility(true,2U,true,false);
  if(mode==1U)Charging_SetEligibility(true,2U,false,true);
  if(mode==2U){regs[0x14]=8U;Charging_OnInterrupt();run(40);Charging_SetEligibility(true,2U,false,false);}
  run(120);assert(!(regs[3]&MP2724_IPRE_MASK));
 }
}
static void completed_precharge_return_does_not_rearm(void)
{
 Charging_Profile p=profile();reset(&p);ready();run(40);
 regs[0x13]=MP2724_CHG_STAT_DONE<<MP2724_CHG_STAT_SHIFT;Charging_OnInterrupt();run(40);
 assert(Charging_GetObservation().completed);
 regs[0x13]=MP2724_CHG_STAT_PRECHARGE<<MP2724_CHG_STAT_SHIFT;Charging_OnInterrupt();run(40);
 Charging_SetEligibility(true,2U,false,false);run(60);
 assert(!(regs[3]&MP2724_IPRE_MASK) && Charging_GetObservation().completed);
}
static void demand_without_adc_cannot_enable(void)
{
 Charging_Profile p=profile();reset(&p);Charging_SetEligibility(false,1U,false,false);ready();run(100);
 assert(!(regs[9]&MP2724_EN_CHG_MASK));
}
static void exhausted_drain_revokes_admission_immediately(void)
{
 for(unsigned arming=0U;arming<2U;arming++) {
  Charging_Profile p=profile();reset(&p);ready();run(40);
  assert(Charging_GetObservation().admitted);
  if(arming) {
   regs[0x13]=MP2724_CHG_STAT_PRECHARGE<<MP2724_CHG_STAT_SHIFT;
   Charging_OnInterrupt();run(40);Charging_SetEligibility(true,2U,false,false);
  } else Charging_OnInterrupt();
  for(unsigned i=0;i<100U;i++) {
   Charging_Update(available);
   if(pending && (arming ? (writing && reg==3U) : (!writing && reg==1U && leased)))break;
   if(pending)complete();
   FakeHAL_AdvanceTick(1U);
  }
  assert(pending);uint8_t *owned_buffer=buffer;bool owned_lease=leased;
  drain_ready=false;run(35U);
  I2C_DeviceResult result=MP2724_GetResult();
  assert(result.exhausted && result.recovering);
  Charging_Observation o=Charging_GetObservation();
  assert(o.communication_fault && o.recovering && !o.admitted);
  assert(!o.active_charging && !o.safe_baseline_ready && !o.configuration_ready);
  assert(pending && buffer==owned_buffer && leased==owned_lease);
  unsigned count=trace_count;run(20U);assert(trace_count==count);
  drain_ready=true;run(40U);
  assert(!Charging_GetObservation().admitted && !Charging_GetObservation().safe_baseline_ready);
 }
}
static void stale_input_completion_cannot_restore_fault_readiness(void)
{
 for(unsigned write=0U;write<2U;write++) {
  Charging_Profile p=profile();reset(&p);Charging_SetChargeRequired(false);ready();run(40);
  if(write)regs[1]=31U;
  Charging_OnInterrupt();
  for(unsigned i=0U;i<200U;i++) {
   Charging_Update(available);
   if(pending && reg==1U && leased && writing==(write!=0U))break;
   if(pending)complete();
   FakeHAL_AdvanceTick(1U);
  }
  assert(pending && reg==1U && leased);complete();
  Charging_SetEligibility(true,2U,false,true);run(30U);
  assert(!Charging_GetObservation().configuration_ready);
 }
}
static void exhausted_transport_cannot_finish_sleep(void)
{
 Charging_Profile p=profile();reset(&p);ready();run(40U);
 Charging_RequestPrepareSleep();fail_reads=true;run(200U);
 assert(Charging_GetObservation().communication_fault);
 assert(!Charging_GetObservation().sleep_ready);
 fail_reads=false;Charging_BeginValidation();run(200U);
 assert(Charging_GetObservation().communication_fault && !Charging_GetObservation().safe_baseline_ready);
 Charging_RequestPrepareSleep();run(100U);assert(!Charging_GetObservation().sleep_ready);
}
static void begin_precharge_arm(const Charging_Profile *p)
{
 reset(p);ready();run(40U);
 regs[0x13]=MP2724_CHG_STAT_PRECHARGE<<MP2724_CHG_STAT_SHIFT;
 Charging_OnInterrupt();run(40U);Charging_SetEligibility(true,2U,false,false);
}
/* Stop at each physical arming write, on both sides of transmission. */
static void revocation_at_each_arming_write(void)
{
 const uint8_t stage_regs[]={9U,0U,3U,0U,9U};
 for(unsigned stage=0U;stage<5U;stage++)for(unsigned after=0U;after<2U;after++)
 for(unsigned mode=0U;mode<7U;mode++) {
  Charging_Profile p=profile();begin_precharge_arm(&p);
  unsigned seen=0U;
  for(unsigned i=0U;i<150U;i++) {
   Charging_Update(available);
   if(pending && writing) {
    if(seen==stage)break;
    seen++;
   }
   if(pending)complete();
   FakeHAL_AdvanceTick(1U);
  }
  assert(pending && writing && reg==stage_regs[stage]);
  if(after)complete();
  if(mode==0U)Charging_SetEligibility(false,3U,false,false);
  if(mode==1U)Charging_SetEligibility(true,3U,true,false);
  if(mode==2U)Charging_SetEligibility(true,3U,false,true);
  if(mode==3U)Charging_RequestPrepareSleep();
  if(mode==4U)Charging_BeginValidation();
  if(mode==5U){regs[0x12]=0U;Charging_OnInterrupt();}
  if(mode==6U){regs[0x14]=8U;Charging_OnInterrupt();}
  if(mode<5U) {
   Charging_Observation o=Charging_GetObservation();
   assert(!o.admitted && o.inhibit_requested && o.precharge_target_ma==0U);
   if(!after) {
    drain_ready=false;run(3U);
    assert(!Charging_GetObservation().sleep_ready && pending);
    drain_ready=true;
   }
  }
  run(180U);
  Charging_Observation o=Charging_GetObservation();
  assert(o.precharge_current_known && o.precharge_current_ma==0U);
  assert(o.parameter_lock_known && o.parameter_locked);
  if(mode!=1U && mode!=4U)assert(o.inhibit_known && o.inhibited && !o.admitted);
  if(mode==3U)assert(o.sleep_ready && o.safe_baseline_ready && !(regs[7]&MP2724_WATCHDOG_MASK));
  if(mode==2U)assert(!o.configuration_ready);
  if(mode==4U)assert(o.configuration_ready);
  /* Requalifying the same battery generation cannot reuse the old epoch. */
  if(mode!=2U && mode!=3U) {
   Charging_SetEligibility(false,3U,false,false);run(40U);
   regs[0x12]=0x60U;regs[0x14]=0U;
   Charging_SetEligibility(true,3U,false,false);Charging_OnInterrupt();run(180U);
   assert(Charging_GetObservation().precharge_target_ma==0U);
   Charging_SetEligibility(true,4U,false,false);run(80U);
   assert(Charging_GetObservation().precharge_current_ma==CHARGER_PRECHARGE_OPERATING_CURRENT_MA);
  }
 }
}
static void unknown_observations_after_exhaustion(void)
{
 Charging_Profile p=profile();begin_precharge_arm(&p);
 for(unsigned i=0U;i<100U;i++) {
  Charging_Update(available);
  if(pending && writing && reg==3U)break;
  if(pending)complete();
  FakeHAL_AdvanceTick(1U);
 }
 drain_ready=false;run(35U);
 Charging_Observation o=Charging_GetObservation();
 assert(o.inhibit_requested && o.precharge_target_ma==0U);
 assert(!o.inhibit_known && !o.inhibited && !o.precharge_current_known && !o.parameter_lock_known);
 Charging_RequestPrepareSleep();drain_ready=true;run(100U);
 o=Charging_GetObservation();assert(o.communication_fault && !o.sleep_ready && !o.inhibit_known);
}
static void failed_parameter_transactions_latch(void)
{
 const uint8_t stage_regs[]={9U,3U,9U,0U,3U,0U,9U};
 const uint8_t stage_masks[]={MP2724_EN_CHG_MASK,MP2724_IPRE_MASK,MP2724_EN_CHG_MASK,
  MP2724_LOCK_CHG_MASK,MP2724_IPRE_MASK,MP2724_LOCK_CHG_MASK,MP2724_EN_CHG_MASK};
 for(unsigned stage=0U;stage<7U;stage++)for(unsigned failure=0U;failure<3U;failure++) {
  Charging_Profile p=profile();
  if(stage<2U){reset(&p);regs[3]=0xf3U;}
  else begin_precharge_arm(&p);
  unsigned target=stage<2U?stage:stage-2U,seen=0U;
  for(unsigned i=0U;i<150U;i++) {
   Charging_Update(available);
   if(pending && writing) {
    if(seen==target)break;
    seen++;
   }
   if(pending)complete();
   FakeHAL_AdvanceTick(1U);
  }
  assert(pending && writing && reg==stage_regs[stage]);
  bool wrote=false;unsigned failures=0U;
  /* The request stays owned across successful prereads and failed retries. */
  for(unsigned i=0U;i<100U;i++) {
   if(pending) {
    assert(reg==stage_regs[stage]);
    if(failure==0U && writing) {failures++;MP2724_OnError(&handle);}
    else if(failure!=0U && !writing && wrote) {
     failures++;wrote=false;
     if(failure==1U)MP2724_OnError(&handle);
     else {*buffer=regs[reg]^stage_masks[stage];pending=false;MP2724_OnReadComplete(&handle);}
    } else {wrote=writing;complete();}
   }
   FakeHAL_AdvanceTick(1U);Charging_Update(available);
   if(Charging_GetObservation().communication_fault)break;
  }
  assert(failures==I2C_DEVICE_FAILURE_LIMIT);
  Charging_Observation o=Charging_GetObservation();
  assert(o.communication_fault && !o.admitted && o.inhibit_requested && o.precharge_target_ma==0U);
  assert(!o.inhibit_known && !o.precharge_current_known && !o.parameter_lock_known);
  unsigned before=trace_count;
  Charging_BeginValidation();Charging_SetEligibility(true,9U,false,false);run(100U);
  Charging_RequestPrepareSleep();run(100U);
  assert(trace_count==before && !Charging_GetObservation().sleep_ready);
 }
}
static void watchdog_rollover_and_restart_active_device(void)
{
 Charging_Profile p=profile();reset(&p);FakeHAL_AdvanceTick(UINT32_MAX-200U);
 regs[3]=p.registers[3];regs[9]|=MP2724_EN_CHG_MASK;
 regs[0x13]=MP2724_CHG_STAT_PRECHARGE<<MP2724_CHG_STAT_SHIFT;
 Charging_SetEligibility(false,1U,false,false);ready();run(150U);
 assert(Charging_GetObservation().inhibited && Charging_GetObservation().precharge_current_ma==0U);
 unsigned before=watchdog_resets;run(CHARGER_WATCHDOG_SERVICE_INTERVAL_MS+100U);
 assert(watchdog_resets>before && !Charging_GetObservation().watchdog_fault);
 Charging_SetEligibility(true,2U,false,false);run(180U);
 assert(Charging_GetObservation().precharge_current_ma==0U);
 Charging_SetEligibility(true,3U,false,false);run(80U);
 assert(Charging_GetObservation().precharge_current_ma==CHARGER_PRECHARGE_OPERATING_CURRENT_MA);
 unsigned fault_reads=status_reads[2];regs[0x13]|=MP2724_CHG_FAULT_MASK;
 for(unsigned i=0U;i<80U;i++){Charging_OnInterrupt();step();}
 assert(status_reads[2]>fault_reads && Charging_GetObservation().charger_fault);
 assert(Charging_GetObservation().inhibited && Charging_GetObservation().precharge_current_ma==0U);
}
static void fault_reconciles_unadmitted_device(void)
{
 for(unsigned application=0U;application<2U;application++) {
  Charging_Profile p=profile();reset(&p);Charging_SetChargeRequired(false);ready();run(40U);
  regs[9]|=MP2724_EN_CHG_MASK;regs[3]=p.registers[3];regs[0]&=~MP2724_LOCK_CHG_MASK;
  if(application)Charging_SetEligibility(false,2U,false,true);
  else {regs[0x13]|=MP2724_CHG_FAULT_MASK;Charging_OnInterrupt();}
  run(100U);
  Charging_Observation o=Charging_GetObservation();
  assert(o.inhibit_known && o.inhibited && o.precharge_current_known && o.precharge_current_ma==0U);
  assert(o.parameter_lock_known && o.parameter_locked && !o.configuration_ready);
 }
}
int main(void)
{
 fault_reconciles_unadmitted_device();
 failed_parameter_transactions_latch();
 watchdog_rollover_and_restart_active_device();
 revocation_at_each_arming_write();
 unknown_observations_after_exhaustion();
 stale_input_completion_cannot_restore_fault_readiness();
 exhausted_transport_cannot_finish_sleep();
 exhausted_drain_revokes_admission_immediately();
 completed_precharge_return_does_not_rearm();
 nonprecharge_and_ineligible_never_raise_current();
 admitted_precharge_requires_new_adc();
 demand_without_adc_cannot_enable();
 baseline_cancellation_reconciles_transmitted_write();
 stale_input_completion_cannot_restore_validation_readiness();
 safe_baseline_without_profile_or_lease();
 battery_only_configuration_uses_explicit_idle_lease();
 monitoring_without_profile();verified_configuration_and_idle_lease();polling_interrupts_and_errors();
 watchdog_recovery_and_runtime_fault();warm_completion_and_usb_watchdog();shipping_commands_and_drain();stale_ntc_and_retained_faults();source_changes_and_refresh_fairness();freshness_requires_powered_ntc();
 sleep_cancellation_after_watchdog_write();
 sleep_readiness_during_shipping_drain();
 shipping_cancellation_never_replays();
 puts("charging: all tests passed");return 0;
}
