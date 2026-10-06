#include "fake_hardware.h"
#include <assert.h>
#include <string.h>
ADC_TypeDef hw_adc_regs;
DMA_Channel_TypeDef hw_dma_regs;
I2C_TypeDef hw_i2c1,hw_i2c2;
TIM_TypeDef hw_tim;
ADC_HandleTypeDef hw_adc;
DMA_HandleTypeDef hw_dma;
I2C_HandleTypeDef hw_charger,hw_motor;
TIM_HandleTypeDef hw_timer;
unsigned hw_adc_starts,hw_dma_words,hw_irq_clears,hw_abort_count;
bool hw_dma_enabled,hw_pwm_started,hw_adc_stop_blocked;
I2C_HandleTypeDef *hw_last_i2c;
uint16_t hw_i2c_size,hw_memadd_size;
static uint16_t *dma_destination;
static uint8_t registers[2][256], bus_reg[2];
static bool bus_write[2];
uint32_t hw_pwm_applied;
void FakeHW_Reset(void)
{
 memset(registers,0,sizeof registers);
 FakeHAL_Reset(); memset(&hw_adc_regs,0,sizeof hw_adc_regs); memset(&hw_dma_regs,0,sizeof hw_dma_regs);
 memset(&hw_i2c1,0,sizeof hw_i2c1); memset(&hw_i2c2,0,sizeof hw_i2c2); memset(&hw_tim,0,sizeof hw_tim);
 memset(&hw_adc,0,sizeof hw_adc); memset(&hw_dma,0,sizeof hw_dma);
 memset(&hw_charger,0,sizeof hw_charger); memset(&hw_motor,0,sizeof hw_motor);
 hw_dma.Parent=&hw_adc;
 hw_adc.Instance=ADC1; hw_adc.DMA_Handle=&hw_dma; hw_dma.Instance=DMA1_Channel1;
 hw_adc.Init.Resolution=ADC_RESOLUTION_12B; hw_adc.Init.ScanConvMode=ADC_SCAN_ENABLE;
 hw_adc.Init.NbrOfConversion=2U; hw_adc.Init.DMAContinuousRequests=ENABLE;
 hw_adc.Init.OversamplingMode=ENABLE; hw_adc.Init.Oversampling.Ratio=16U;
 hw_dma.Init.Request=DMA_REQUEST_ADC1; hw_dma.Init.MemInc=DMA_MINC_ENABLE;
 hw_dma.Init.PeriphDataAlignment=DMA_PDATAALIGN_HALFWORD; hw_dma.Init.MemDataAlignment=DMA_MDATAALIGN_HALFWORD;
 hw_dma.Init.Mode=DMA_CIRCULAR; hw_adc_regs.CHSELR=0x81U;
 hw_charger.Instance=I2C1; hw_motor.Instance=I2C2;
 hw_charger.State=hw_motor.State=HAL_I2C_STATE_READY;
 hw_i2c1.CR1=hw_i2c2.CR1=I2C_CR1_PE;
 hw_timer.Instance=TIM1; hw_tim.ARR=999U; hw_tim.CCMR1=TIM_OCMODE_PWM1;
 hw_pwm_applied=0U;
 hw_adc_starts=hw_dma_words=hw_irq_clears=hw_abort_count=0U;
 hw_dma_enabled=hw_pwm_started=hw_adc_stop_blocked=false; dma_destination=NULL;
}
HAL_StatusTypeDef ADC_Enable(ADC_HandleTypeDef *h) {h->Instance->CR|=ADC_CR_ADEN;h->Instance->ISR|=ADC_FLAG_RDY;return HAL_OK;}
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *h) {(void)h;return HAL_OK;}
HAL_StatusTypeDef HAL_DMA_Start_IT(DMA_HandleTypeDef *h,uintptr_t src,uintptr_t dst,uint32_t n)
{ assert(src==(uintptr_t)&hw_adc_regs.DR);dma_destination=(uint16_t *)dst;hw_dma_words=n;
 h->State=HAL_DMA_STATE_BUSY;h->Instance->CCR|=DMA_CCR_EN;h->Instance->CNDTR=n;hw_dma_enabled=true;return HAL_OK; }
HAL_StatusTypeDef HAL_DMA_Abort(DMA_HandleTypeDef *h)
{h->State=HAL_DMA_STATE_READY;h->Instance->CCR&=~DMA_CCR_EN;hw_dma_enabled=false;return HAL_OK;}
static HAL_StatusTypeDef transfer(I2C_HandleTypeDef *h,uint16_t address,uint16_t reg,uint16_t size,uint8_t *b,uint16_t n,bool write)
{ assert(address!=0U && reg<256U);if(h->State!=HAL_I2C_STATE_READY)return HAL_BUSY;
 unsigned bus=h==&hw_charger?0U:1U;bus_reg[bus]=(uint8_t)reg;bus_write[bus]=write;
 hw_last_i2c=h;hw_i2c_size=n;hw_memadd_size=size;h->pBuffPtr=b;h->XferCount=n;
 h->Mode=HAL_I2C_MODE_MEM;h->State=write?HAL_I2C_STATE_BUSY_TX:HAL_I2C_STATE_BUSY_RX;
 h->Instance->ISR|=I2C_FLAG_BUSY;return HAL_OK; }
HAL_StatusTypeDef HAL_I2C_Mem_Read_IT(I2C_HandleTypeDef *h,uint16_t a,uint16_t r,uint16_t s,uint8_t *b,uint16_t n)
{return transfer(h,a,r,s,b,n,false);}
HAL_StatusTypeDef HAL_I2C_Mem_Write_IT(I2C_HandleTypeDef *h,uint16_t a,uint16_t r,uint16_t s,uint8_t *b,uint16_t n)
{return transfer(h,a,r,s,b,n,true);}
HAL_StatusTypeDef HAL_I2C_Master_Abort_IT(I2C_HandleTypeDef *h,uint16_t a)
{(void)a;++hw_abort_count;h->State=HAL_I2C_STATE_ABORT;return HAL_OK;}
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *h,uint32_t c)
{(void)c;assert(h->Instance->CCR1==0U);hw_pwm_started=true;return HAL_OK;}
void HAL_NVIC_ClearPendingIRQ(IRQn_Type irq) {(void)irq;++hw_irq_clears;}
void FakeHW_AdcComplete(unsigned half,uint16_t tip,uint16_t battery)
{assert(dma_destination);dma_destination[half*2U]=tip;dma_destination[half*2U+1U]=battery;
 hw_adc_regs.CR&=~ADC_CR_ADSTART;hw_dma_regs.CNDTR=half==0U?2U:4U;
 if(half==0U)hw_dma.XferHalfCpltCallback(&hw_dma);else hw_dma.XferCpltCallback(&hw_dma);}
void FakeHW_I2cComplete(I2C_HandleTypeDef *h,uint8_t value,bool write)
{assert(h->pBuffPtr);*h->pBuffPtr=value;h->State=HAL_I2C_STATE_READY;h->Mode=HAL_I2C_MODE_NONE;
 h->Instance->ISR&=~I2C_FLAG_BUSY; if(write)HAL_I2C_MemTxCpltCallback(h);else HAL_I2C_MemRxCpltCallback(h);}

void FakeHW_TimerUpdate(TIM_HandleTypeDef *h,uint32_t event) {(void)event;hw_pwm_applied=h->Instance->CCR1;}

HAL_StatusTypeDef HAL_TIM_GenerateEvent(TIM_HandleTypeDef *h,uint32_t e) {FakeHW_TimerUpdate(h,e);return HAL_OK;}

void FakeHW_Service(void)
{
 if(hw_dma_enabled && (hw_adc_regs.CR & ADC_CR_ADSTART)) {
   unsigned half=hw_dma_regs.CNDTR==4U?0U:1U;
   FakeHW_AdcComplete(half,1912U*16U,3276U*16U);
 }
 I2C_HandleTypeDef *handles[2]={&hw_charger,&hw_motor};
 for(unsigned bus=0U;bus<2U;++bus) {
   I2C_HandleTypeDef *h=handles[bus];
   if(h->State!=HAL_I2C_STATE_BUSY_RX && h->State!=HAL_I2C_STATE_BUSY_TX)continue;
   if(bus_write[bus]) registers[bus][bus_reg[bus]]=*h->pBuffPtr;
   FakeHW_I2cComplete(h,registers[bus][bus_reg[bus]],bus_write[bus]);
 }
}
