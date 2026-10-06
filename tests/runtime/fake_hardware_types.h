#ifndef TEST_HW_TYPES_H
#define TEST_HW_TYPES_H
#include <stdint.h>
typedef struct { uint32_t CR, ISR, IER, CFGR1, CHSELR, DR; } ADC_TypeDef;
typedef struct { uint32_t CCR, CNDTR; } DMA_Channel_TypeDef;
typedef struct { uint32_t CR1, CR2, ISR, ICR, RXDR, TXDR; } I2C_TypeDef;
typedef struct { uint32_t CR1, CCMR1, CCER, ARR, CCR1; } TIM_TypeDef;
typedef struct DMA_HandleTypeDef DMA_HandleTypeDef;
struct DMA_HandleTypeDef {
 DMA_Channel_TypeDef *Instance;
 struct {uint32_t Request,Direction,PeriphInc,MemInc,PeriphDataAlignment,MemDataAlignment,Mode;} Init;
 void *Parent;
 struct {uint32_t CSR,CFR;} *DMAmuxChannelStatus;
 uint32_t DMAmuxChannelStatusMask;
 uint32_t State;
 void (*XferCpltCallback)(DMA_HandleTypeDef *);
 void (*XferHalfCpltCallback)(DMA_HandleTypeDef *);
 void (*XferErrorCallback)(DMA_HandleTypeDef *);
};
typedef struct {
 ADC_TypeDef *Instance; DMA_HandleTypeDef *DMA_Handle;
 struct {uint32_t Resolution,DataAlign,ScanConvMode,ContinuousConvMode,NbrOfConversion,
 DiscontinuousConvMode,ExternalTrigConv,ExternalTrigConvEdge,DMAContinuousRequests,
 LowPowerAutoWait,LowPowerAutoPowerOff,OversamplingMode;
 struct {uint32_t Ratio,RightBitShift,TriggeredMode;} Oversampling;} Init;
 uint32_t State, ErrorCode;
} ADC_HandleTypeDef;
typedef struct { I2C_TypeDef *Instance; struct {uint32_t AddressingMode;} Init;
 uint32_t State,Mode,ErrorCode,PreviousState,Lock,XferCount,XferSize,XferOptions;
 uint8_t *pBuffPtr; void (*XferISR)(void); } I2C_HandleTypeDef;
typedef struct {TIM_TypeDef *Instance;} TIM_HandleTypeDef;
typedef unsigned IRQn_Type;
typedef unsigned HAL_StatusTypeDef;
#endif
