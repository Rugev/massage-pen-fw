#ifndef TEST_LL_ADC_H
#define TEST_LL_ADC_H
#include "fake_hardware.h"
static inline unsigned LL_ADC_REG_GetSequencerConfigurable(ADC_TypeDef *a) {(void)a;return 1U;}
static inline unsigned LL_ADC_REG_GetSequencerLength(ADC_TypeDef *a) {(void)a;return 2U;}
static inline unsigned LL_ADC_REG_GetSequencerRanks(ADC_TypeDef *a,unsigned r) {return r==1U ? a->CHSELR&15U : (a->CHSELR>>4)&15U;}
static inline unsigned LL_ADC_GetChannelSamplingTime(ADC_TypeDef *a,unsigned c) {(void)a;(void)c;return 0U;}
static inline unsigned LL_ADC_REG_IsConversionOngoing(ADC_TypeDef *a) {return (a->CR&ADC_CR_ADSTART)!=0U;}
static inline unsigned LL_ADC_IsEnabled(ADC_TypeDef *a) {return (a->CR&ADC_CR_ADEN)!=0U;}
static inline void LL_ADC_REG_StartConversion(ADC_TypeDef *a) {a->CR|=ADC_CR_ADSTART;++hw_adc_starts;}
static inline void LL_ADC_REG_StopConversion(ADC_TypeDef *a) {if(!hw_adc_stop_blocked)a->CR&=~(ADC_CR_ADSTART|ADC_CR_ADSTP);}
static inline void LL_ADC_Disable(ADC_TypeDef *a) {a->CR&=~(ADC_CR_ADEN|ADC_CR_ADDIS);}
#endif
