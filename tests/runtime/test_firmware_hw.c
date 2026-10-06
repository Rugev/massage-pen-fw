#include <assert.h>
#include <stdio.h>
#include "fake_hardware.h"
#include "firmware_hw.h"
#include "mp2724.h"
#include "drv2624.h"
void Charging_OnInterrupt(void) {}
static App_Bindings bindings;
static void start_sensors(void)
{
    Sensors_Init(bindings.adc, bindings.sensors);
    Sensors_Update(true); FakeHAL_AdvanceTick(10U); Sensors_Update(true);
}
/* Binding mismatches must gate rather than silently reinterpret hardware. */
static void test_config_and_pwm(void)
{
    FakeHW_Reset();
    FirmwareHW_Init(&bindings,&hw_adc,&hw_charger,&hw_motor,&hw_timer);
    assert(bindings.sensors && bindings.heater_pwm && bindings.charger_transport && bindings.vibration_transport);
    assert(hw_pwm_started && hw_tim.CCR1==0U && hw_adc_starts==0U);
    bindings.heater_pwm->set_duty(bindings.heater_context,500000U); assert(hw_tim.CCR1==500U);
    bindings.heater_pwm->set_duty(bindings.heater_context,1000000U); assert(hw_tim.CCR1==1000U);
    FakeHW_TimerUpdate(&hw_timer,1U); assert(hw_pwm_applied==1000U);
    bindings.heater_pwm->set_duty(bindings.heater_context,0U); assert(hw_tim.CCR1==0U && hw_pwm_applied==0U);
    uint16_t frames[4]={0};
    assert(bindings.sensors->arm(&hw_adc,frames,4U));
    assert(hw_adc_starts==0U && hw_dma_enabled); /* arm must not ADSTART */
    assert(bindings.sensors->trigger(&hw_adc) && hw_adc_starts==1U);
    while(!bindings.sensors->stop_quiescent(&hw_adc)) {}
    hw_adc.Init.ContinuousConvMode=ENABLE;
    FirmwareHW_Init(&bindings,&hw_adc,&hw_charger,&hw_motor,&hw_timer); assert(!bindings.sensors);
    FakeHW_Reset(); hw_dma.Parent=NULL;
    FirmwareHW_Init(&bindings,&hw_adc,&hw_charger,&hw_motor,&hw_timer); assert(!bindings.sensors);
    FakeHW_Reset(); hw_adc_regs.CHSELR=0U; hw_tim.CCER=TIM_CCER_CC1P;
    FirmwareHW_Init(&bindings,&hw_adc,&hw_charger,&hw_motor,&hw_timer);
    assert(!bindings.sensors && !bindings.heater_pwm && !hw_pwm_started);
}
/* DMA raw tip/battery must become matched battery/tip, with exact one trigger. */
static void test_adc_pairing_and_drain(void)
{
    FakeHW_Reset(); FirmwareHW_Init(&bindings,&hw_adc,&hw_charger,&hw_motor,&hw_timer);
    start_sensors(); assert(hw_adc_starts==1U && hw_dma_words==4U);
    /* arm alone never starts a conversion; also reject overlap. */
    assert(!bindings.sensors->trigger(&hw_adc)); assert(hw_adc_starts==1U);
    FakeHW_AdcComplete(0U,1912U*16U,3276U*16U);
    FakeHAL_AdvanceTick(1U); Sensors_Update(true);
    assert(Sensors_GetSnapshot().fresh && Sensors_GetSnapshot().battery_mv==4000U);
    assert(Sensors_GetSnapshot().tip_mdegc==24995L && hw_adc_starts==2U);
    FakeHW_AdcComplete(1U,1435U*16U,2457U*16U);
    FakeHAL_AdvanceTick(1U); Sensors_Update(true);
    assert(Sensors_GetSnapshot().battery_mv==3000U && hw_adc_starts==3U);
    ADC_HandleTypeDef foreign={0}; HAL_ADC_ErrorCallback(&foreign);
    assert(!Sensors_GetSnapshot().acquisition_fault);
    hw_adc_stop_blocked=true; Sensors_Update(false);
    assert(!Sensors_IsQuiescent());
    FakeHW_AdcComplete(0U,60000U,60000U); /* Cancelled ISR cannot publish. */
    hw_adc_stop_blocked=false; FakeHAL_AdvanceTick(1U); Sensors_Update(false);
    assert(Sensors_IsQuiescent() && !hw_dma_enabled && hw_irq_clears>0U);
    start_sensors(); assert(hw_adc_starts==4U);
    FakeHW_AdcComplete(0U,1912U*16U,3276U*16U);
    FakeHAL_AdvanceTick(1U); Sensors_Update(true); assert(Sensors_GetSnapshot().battery_mv==4000U);
}
/* Real driver engine receives only its own accepted callbacks; hung abort is bounded. */
static void test_i2c_routing_and_abort(void)
{
    FakeHW_Reset(); FirmwareHW_Init(&bindings,&hw_adc,&hw_charger,&hw_motor,&hw_timer);
    MP2724_Init(bindings.charger_i2c,bindings.charger_transport);
    DRV2624_Init(bindings.vibration_i2c,bindings.vibration_transport);
    assert(MP2724_RequestRead(MP2724_REG_STATUS0)); MP2724_Update();
    assert(hw_last_i2c==&hw_charger && hw_i2c_size==1U && hw_memadd_size==I2C_MEMADD_SIZE_8BIT);
    I2C_HandleTypeDef foreign={0}; HAL_I2C_MemRxCpltCallback(&foreign); MP2724_Update();
    assert(MP2724_GetResult().state==I2C_RESULT_PENDING);
    FakeHW_I2cComplete(&hw_charger,0x42U,false); MP2724_Update();
    assert(MP2724_GetResult().state==I2C_RESULT_READ && MP2724_GetResult().value==0x42U);
    assert(MP2724_Acknowledge());
    assert(DRV2624_RequestRead(DRV2624_REG_MODE)); DRV2624_Update();
    HAL_I2C_MemRxCpltCallback(&hw_charger); DRV2624_Update();
    assert(DRV2624_GetResult().state==I2C_RESULT_PENDING);
    FakeHW_I2cComplete(&hw_motor,0x24U,false); DRV2624_Update();
    assert(DRV2624_GetResult().state==I2C_RESULT_READ && DRV2624_GetResult().value==0x24U);
    assert(MP2724_RequestRead(MP2724_REG_STATUS0)); MP2724_Update();
    MP2724_Cancel(); MP2724_Update(); assert(hw_abort_count==1U && MP2724_GetResult().recovering);
    MP2724_Update(); assert(hw_abort_count==1U);
    HAL_I2C_MemRxCpltCallback(&hw_charger); /* Late success during abort ignored. */
    FakeHAL_AdvanceTick(5U); MP2724_Update();
    assert(MP2724_GetResult().state==I2C_RESULT_CANCELLED && !MP2724_GetResult().recovering);
    assert(hw_charger.pBuffPtr==NULL && hw_charger.XferISR==NULL);
    assert(MP2724_Acknowledge());
    assert(MP2724_RequestRead(MP2724_REG_STATUS0)); MP2724_Update();
    FakeHW_I2cComplete(&hw_charger,0x11U,false); MP2724_Update();
    assert(MP2724_GetResult().value==0x11U);
    assert(MP2724_Acknowledge());
    assert(MP2724_RequestRead(MP2724_REG_STATUS0)); MP2724_Update();
    HAL_I2C_MemTxCpltCallback(&hw_charger); /* wrong direction cannot consume read */
    FakeHW_I2cComplete(&hw_charger,0x33U,false); MP2724_Update();
    assert(MP2724_GetResult().state==I2C_RESULT_READ && MP2724_GetResult().value==0x33U);
    assert(MP2724_Acknowledge());
    assert(MP2724_RequestRead(MP2724_REG_STATUS0)); MP2724_Update();
    MP2724_Cancel(); MP2724_Update();
    hw_charger.State=HAL_I2C_STATE_READY; hw_charger.Mode=HAL_I2C_MODE_NONE;
    HAL_I2C_AbortCpltCallback(&hw_charger); MP2724_Update();
    assert(MP2724_GetResult().state==I2C_RESULT_CANCELLED && !MP2724_GetResult().recovering);
    assert(MP2724_Acknowledge());
    /* Error callback reaches only the owner and cannot yield a successful read. */
    assert(MP2724_RequestRead(MP2724_REG_STATUS0)); MP2724_Update();
    HAL_I2C_ErrorCallback(&hw_charger); MP2724_Update();
    assert(MP2724_GetResult().failures==1U && MP2724_GetResult().recovering);
}
int main(void)
{
    /* callbacks before binding must be harmless */
    HAL_ADC_ConvHalfCpltCallback(&hw_adc); HAL_I2C_ErrorCallback(&hw_charger);
    test_config_and_pwm(); test_adc_pairing_and_drain(); test_i2c_routing_and_abort();
    puts("hardware adapter tests passed");
}
