#include "fake_hal.h"
#include "main.h"
#include <string.h>
GPIO_TypeDef fake_gpioa, fake_gpiob, fake_gpioc;
static uint32_t fake_tick;
static uint16_t inputs[3], outputs[3];
static unsigned index_of(GPIO_TypeDef *p) { return p == GPIOA ? 0U : p == GPIOB ? 1U : 2U; }
void HAL_GPIO_WritePin(GPIO_TypeDef *p, uint16_t pin, GPIO_PinState s)
{ if (s) outputs[index_of(p)] |= pin; else outputs[index_of(p)] &= (uint16_t)~pin; }
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *p, uint16_t pin)
{ return (inputs[index_of(p)] & pin) ? GPIO_PIN_SET : GPIO_PIN_RESET; }
void FakeHAL_SetInput(GPIO_TypeDef *p, uint16_t pin, GPIO_PinState s)
{ if (s) inputs[index_of(p)] |= pin; else inputs[index_of(p)] &= (uint16_t)~pin; }
GPIO_PinState FakeHAL_GetOutput(GPIO_TypeDef *p, uint16_t pin)
{ return (outputs[index_of(p)] & pin) ? GPIO_PIN_SET : GPIO_PIN_RESET; }
uint32_t HAL_GetTick(void) { return fake_tick; }
void FakeHAL_Reset(void) { fake_tick=0U; memset(inputs,0,sizeof inputs); memset(outputs,0,sizeof outputs); }
void FakeHAL_SetTick(uint32_t t) { fake_tick=t; }
void FakeHAL_AdvanceTick(uint32_t d) { fake_tick+=d; }
void FakeHAL_SetPowerGood(GPIO_PinState s) { FakeHAL_SetInput(SYS_PG_GPIO_Port,SYS_PG_Pin,s); }
GPIO_PinState FakeHAL_GetSysOn(void) { return FakeHAL_GetOutput(SYS_ON_GPIO_Port,SYS_ON_Pin); }
