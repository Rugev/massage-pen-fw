#ifndef TEST_FAKE_HW_H
#define TEST_FAKE_HW_H
#include "fake_hal.h"
#include <stddef.h>
#define ENABLE 1U
#define DISABLE 0U
#define HAL_OK 0U
#define HAL_ERROR 1U
#define HAL_BUSY 2U
#define SET 1U
#define RESET 0U
#define READ_BIT(r,b) ((r)&(b))
#define SET_BIT(r,b) ((r)|=(b))
#define CLEAR_BIT(r,b) ((r)&=~(b))
#define MODIFY_REG(r,c,s) ((r)=((r)&~(c))|(s))
#define __NOP() ((void)0)
#define __DSB() ((void)0)
#define ADC_RESOLUTION_12B 12U
#define ADC_DATAALIGN_RIGHT 0U
#define ADC_SCAN_ENABLE 1U
#define ADC_SOFTWARE_START 0U
#define ADC_EXTERNALTRIGCONVEDGE_NONE 0U
#define ADC_OVERSAMPLING_RATIO_16 16U
#define ADC_RIGHTBITSHIFT_NONE 0U
#define ADC_TRIGGEREDMODE_SINGLE_TRIGGER 0U
#define DMA_REQUEST_ADC1 5U
#define DMA_PERIPH_TO_MEMORY 0U
#define DMA_PINC_DISABLE 0U
#define DMA_MINC_ENABLE 1U
#define DMA_PDATAALIGN_HALFWORD 2U
#define DMA_MDATAALIGN_HALFWORD 2U
#define DMA_CIRCULAR 1U
#define HAL_DMA_STATE_BUSY 1U
#define HAL_DMA_STATE_READY 0U
#define ADC_CFGR1_DMAEN 1U
#define ADC_FLAG_EOC 2U
#define ADC_FLAG_EOS 4U
#define ADC_FLAG_OVR 8U
#define ADC_FLAG_RDY 16U
#define ADC_IT_OVR ADC_FLAG_OVR
#define ADC_CR_ADEN 1U
#define ADC_CR_ADSTART 2U
#define ADC_CR_ADSTP 4U
#define ADC_CR_ADDIS 8U
#define HAL_ADC_STATE_READY 1U
#define HAL_ADC_STATE_REG_BUSY 2U
#define HAL_ADC_ERROR_NONE 0U
#define DMA_CCR_EN 1U
#define DMA_CCR_HTIE 2U
#define DMA_CCR_TCIE 4U
#define DMA_CCR_TEIE 8U
#define LL_ADC_REG_SEQ_CONFIGURABLE 1U
#define LL_ADC_REG_SEQ_SCAN_ENABLE_2RANKS 2U
#define LL_ADC_REG_RANK_1 1U
#define LL_ADC_REG_RANK_2 2U
#define LL_ADC_CHANNEL_1 1U
#define LL_ADC_CHANNEL_8 8U
#define LL_ADC_SAMPLINGTIME_COMMON_1 0U
#define I2C_ADDRESSINGMODE_7BIT 0U
#define I2C_MEMADD_SIZE_8BIT 1U
#define HAL_I2C_STATE_READY 0U
#define HAL_I2C_STATE_BUSY_TX 1U
#define HAL_I2C_STATE_BUSY_RX 2U
#define HAL_I2C_STATE_ABORT 3U
#define HAL_I2C_MODE_NONE 0U
#define HAL_I2C_MODE_MEM 1U
#define HAL_I2C_MODE_MASTER 2U
#define HAL_I2C_ERROR_NONE 0U
#define HAL_UNLOCKED 0U
#define I2C_CR1_PE 1U
#define I2C_CR1_TXIE 2U
#define I2C_CR1_RXIE 4U
#define I2C_CR1_ADDRIE 8U
#define I2C_CR1_NACKIE 16U
#define I2C_CR1_STOPIE 32U
#define I2C_CR1_TCIE 64U
#define I2C_CR1_ERRIE 128U
#define I2C_CR1_TXDMAEN 256U
#define I2C_CR1_RXDMAEN 512U
#define I2C_FLAG_BUSY 1U
#define I2C_FLAG_RXNE 2U
#define I2C_FLAG_TXE 4U
#define I2C_FLAG_STOPF 8U
#define I2C_FLAG_AF 16U
#define I2C_FLAG_BERR 32U
#define I2C_FLAG_ARLO 64U
#define I2C_FLAG_OVR 128U
#define TIM_CCMR1_OC1M 0x70U
#define TIM_OCMODE_PWM1 0x60U
#define TIM_CCER_CC1P 2U
#define TIM_CR1_DIR 16U
#define TIM_CR1_CMS 96U
#define TIM_CHANNEL_1 0U
#define TIM_EVENTSOURCE_UPDATE 1U
#define ADC1_IRQn 1U
#define DMA1_Channel1_IRQn 2U
#define I2C1_IRQn 3U
#define I2C2_IRQn 4U
extern ADC_TypeDef hw_adc_regs;
extern DMA_Channel_TypeDef hw_dma_regs;
extern I2C_TypeDef hw_i2c1,hw_i2c2;
extern TIM_TypeDef hw_tim;
extern ADC_HandleTypeDef hw_adc;
extern DMA_HandleTypeDef hw_dma;
extern I2C_HandleTypeDef hw_charger,hw_motor;
extern TIM_HandleTypeDef hw_timer;
#define ADC1 (&hw_adc_regs)
#define DMA1_Channel1 (&hw_dma_regs)
#define I2C1 (&hw_i2c1)
#define I2C2 (&hw_i2c2)
#define TIM1 (&hw_tim)
#define __HAL_ADC_CLEAR_FLAG(h,f) ((h)->Instance->ISR &= ~(f))
#define __HAL_ADC_ENABLE_IT(h,f) ((h)->Instance->IER |= (f))
#define __HAL_ADC_DISABLE_IT(h,f) ((h)->Instance->IER &= ~(f))
#define __HAL_DMA_GET_GI_FLAG_INDEX(h) 15U
#define __HAL_DMA_GET_TE_FLAG_INDEX(h) 8U
#define __HAL_DMA_GET_HT_FLAG_INDEX(h) 2U
#define __HAL_DMA_GET_TC_FLAG_INDEX(h) 4U
#define __HAL_DMA_CLEAR_FLAG(h,f) ((void)(h),(void)(f))
#define __HAL_DMA_GET_FLAG(h,f) (0U)
#define __HAL_DMA_GET_COUNTER(h) ((h)->Instance->CNDTR)
#define __HAL_I2C_GET_FLAG(h,f) (((h)->Instance->ISR&(f)) != 0U)
#define __HAL_I2C_CLEAR_FLAG(h,f) ((h)->Instance->ISR &= ~(f))
#define __HAL_I2C_DISABLE(h) ((h)->Instance->CR1 &= ~I2C_CR1_PE)
#define __HAL_I2C_ENABLE(h) ((h)->Instance->CR1 |= I2C_CR1_PE)
#define __HAL_TIM_SET_COMPARE(h,c,v) ((void)(c),(h)->Instance->CCR1=(v))
#define __HAL_TIM_GET_AUTORELOAD(h) ((h)->Instance->ARR)
HAL_StatusTypeDef HAL_TIM_GenerateEvent(TIM_HandleTypeDef *,uint32_t);
void FakeHW_TimerUpdate(TIM_HandleTypeDef *, uint32_t);
extern uint32_t hw_pwm_applied;
HAL_StatusTypeDef ADC_Enable(ADC_HandleTypeDef *h);
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *h);
HAL_StatusTypeDef HAL_DMA_Start_IT(DMA_HandleTypeDef *,uintptr_t,uintptr_t,uint32_t);
HAL_StatusTypeDef HAL_DMA_Abort(DMA_HandleTypeDef *);
HAL_StatusTypeDef HAL_I2C_Mem_Read_IT(I2C_HandleTypeDef *,uint16_t,uint16_t,uint16_t,uint8_t *,uint16_t);
HAL_StatusTypeDef HAL_I2C_Mem_Write_IT(I2C_HandleTypeDef *,uint16_t,uint16_t,uint16_t,uint8_t *,uint16_t);
HAL_StatusTypeDef HAL_I2C_Master_Abort_IT(I2C_HandleTypeDef *,uint16_t);
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *,uint32_t);
void HAL_NVIC_ClearPendingIRQ(IRQn_Type);
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *);
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *);
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *);
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *);
void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *);
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *);
void HAL_I2C_AbortCpltCallback(I2C_HandleTypeDef *);
extern unsigned hw_adc_starts,hw_dma_words,hw_irq_clears,hw_abort_count;
extern bool hw_dma_enabled,hw_pwm_started,hw_adc_stop_blocked;
extern I2C_HandleTypeDef *hw_last_i2c;
extern uint16_t hw_i2c_size,hw_memadd_size;
void FakeHW_Reset(void);
void FakeHW_Service(void);
void FakeHW_AdcComplete(unsigned half,uint16_t tip,uint16_t battery);
void FakeHW_I2cComplete(I2C_HandleTypeDef *,uint8_t,bool);
#endif
