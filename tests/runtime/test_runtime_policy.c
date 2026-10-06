/* Complete runtime + real policy/mechanisms; device registers are synthetic. */
#include "runtime.h"
#include "fake_hardware.h"
#include "main.h"
#include "mp2724.h"
#include <assert.h>
#include <stdio.h>
static bool leased;
static bool acquire(uint8_t reg) { (void)reg; assert(!leased); leased=true; return true; }
static void release(uint8_t reg) { (void)reg; assert(leased); leased=false; }
static void tick(void)
{
    FakeHAL_AdvanceTick(1U); Runtime_Poll(); FakeHW_Service();
}
static void ticks(unsigned count) { while(count-- != 0U) tick(); }
int main(void)
{
    static const Charging_IdleOps idle={acquire,release};
    static const Charging_Profile charging={.agreed=true,
      .registers={0x10,0,(MP2724_VPRE_3000_MV << MP2724_VPRE_SHIFT) | 0x23, ((CHARGER_PRECHARGE_OPERATING_CURRENT_MA / MP2724_IPRE_STEP_MA) << MP2724_IPRE_SHIFT) | 3U,0x06,0x18,0x04,0x1e,0x20,0x03,0x20,0x51,0x21,0x4e,0,0},.idle=&idle};
    static const Vibration_Profile motor={.validated=true,.mode=0x08,.control=0x80,.feedback_control=0x52,
      .rated_voltage=0x33,.od_clamp=0x44,.lra_drive_control=0x07,.bemf_timing=0x22,
      .timing_control=0x0c,.auto_cal_time=0,.calibration_duration_ms=250,.calibration_timeout_ms=400,.rtp={0,31,63,127}};
    const Runtime_Profiles profiles={&charging,&motor};
    FakeHW_Reset(); FakeHAL_SetPowerGood(GPIO_PIN_SET);
    FakeHAL_SetInput(CHRG_INT_GPIO_Port,CHRG_INT_Pin,GPIO_PIN_SET);
    Runtime_Poll(); assert(hw_adc_starts==0U);
    Runtime_Init(&hw_adc,&hw_charger,&hw_motor,&hw_timer,&profiles);
    ticks(1000U);
    assert(App_GetSnapshot().state==APP_SLEEP && Runtime_GetObservation().polling_sleep);
    assert(Sensors_IsQuiescent() && FakeHAL_GetSysOn()==GPIO_PIN_RESET && !leased);
    unsigned triggers=hw_adc_starts; ticks(50U); assert(hw_adc_starts==triggers);
    /* Raw bounce is rejected by existing wake debounce/hold policy. */
    FakeHAL_SetInput(BUTTON_PWR_ON_GPIO_Port,BUTTON_PWR_ON_Pin,GPIO_PIN_SET); Runtime_Poll();
    ticks(5U); FakeHAL_SetInput(BUTTON_PWR_ON_GPIO_Port,BUTTON_PWR_ON_Pin,GPIO_PIN_RESET);
    ticks(1000U); assert(App_GetSnapshot().state==APP_SLEEP && App_GetSnapshot().fault_code==0U);
    /* New sustained waking press enables through the real App API. */
    FakeHAL_SetInput(BUTTON_PWR_ON_GPIO_Port,BUTTON_PWR_ON_Pin,GPIO_PIN_SET); Runtime_Poll();
    for(unsigned i=0U;i<1000U && App_GetSnapshot().state!=APP_NORMAL;++i) tick();
    assert(App_GetSnapshot().state==APP_NORMAL && App_GetSnapshot().fault_code==0U);
    Runtime_Init(NULL,NULL,NULL,NULL,NULL); assert(App_GetSnapshot().state==APP_NORMAL);
    FakeHAL_SetPowerGood(GPIO_PIN_RESET); tick();
    assert(App_GetSnapshot().state==APP_FAULT_DISPLAY && App_GetSnapshot().fault_code==5U);
    assert(FakeHAL_GetSysOn()==GPIO_PIN_RESET && hw_tim.CCR1==0U && hw_pwm_applied==0U);
    ticks(60010U);
    assert(App_GetSnapshot().state==APP_FAULT_SLEEP && Runtime_GetObservation().polling_sleep);
    triggers=hw_adc_starts; ticks(20U); assert(hw_adc_starts==triggers);
    /* Held enabling/fault button cannot wake; charger episode retains latch. */
    assert(App_GetSnapshot().state==APP_FAULT_SLEEP);
    FakeHAL_SetInput(CHRG_INT_GPIO_Port,CHRG_INT_Pin,GPIO_PIN_RESET); Runtime_Poll();
    assert(App_GetSnapshot().state==APP_FAULT_DISPLAY && App_GetSnapshot().fault_code==5U);
    assert(FakeHAL_GetSysOn()==GPIO_PIN_RESET);
    ticks(10010U); assert(App_GetSnapshot().state==APP_FAULT_SLEEP);
    FakeHAL_SetInput(BUTTON_PWR_ON_GPIO_Port,BUTTON_PWR_ON_Pin,GPIO_PIN_RESET); Runtime_Poll();
    FakeHAL_SetInput(BUTTON_PWR_ON_GPIO_Port,BUTTON_PWR_ON_Pin,GPIO_PIN_SET); Runtime_Poll();
    assert(App_GetSnapshot().state==APP_FAULT_DISPLAY && App_GetSnapshot().fault_code==5U);
    assert(FakeHAL_GetSysOn()==GPIO_PIN_RESET);
    puts("runtime real-policy sleep/wake/fault retention passed");
}
