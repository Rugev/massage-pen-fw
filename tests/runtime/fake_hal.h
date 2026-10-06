#ifndef TEST_FAKE_HAL_H
#define TEST_FAKE_HAL_H

#include <stdint.h>
#include <stdbool.h>

typedef uint16_t GPIO_PinState;
typedef struct { uint32_t unused; } GPIO_TypeDef;
#ifdef RUNTIME_HW_TEST
#include "fake_hardware_types.h"
#else
typedef struct { uint32_t unused; } TIM_HandleTypeDef;
typedef struct { uint32_t unused; } ADC_HandleTypeDef;
typedef struct { uint32_t unused; } I2C_HandleTypeDef;
#endif

static inline uint32_t __get_PRIMASK(void) { return 0U; }
static inline void __disable_irq(void) {}
static inline void __set_PRIMASK(uint32_t mask) { (void)mask; }

#define GPIO_PIN_RESET 0U
#define GPIO_PIN_SET   1U
#define GPIO_PIN_0 (1U << 0)
#define GPIO_PIN_1 (1U << 1)
#define GPIO_PIN_2 (1U << 2)
#define GPIO_PIN_3 (1U << 3)
#define GPIO_PIN_4 (1U << 4)
#define GPIO_PIN_5 (1U << 5)
#define GPIO_PIN_8 (1U << 8)
#define GPIO_PIN_9 (1U << 9)
#define GPIO_PIN_10 (1U << 10)
#define GPIO_PIN_11 (1U << 11)
#define GPIO_PIN_12 (1U << 12)
#define GPIO_PIN_13 (1U << 13)
#define GPIO_PIN_14 (1U << 14)
#define GPIO_PIN_15 (1U << 15)

#define GPIO_PIN_6     (1U << 6)
#define GPIO_PIN_7     (1U << 7)

extern GPIO_TypeDef fake_gpioa, fake_gpiob, fake_gpioc;
#define GPIOB (&fake_gpiob)
#define GPIOC (&fake_gpioc)
#define GPIOA (&fake_gpioa)

void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin);
uint32_t HAL_GetTick(void);

void FakeHAL_SetInput(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state);
GPIO_PinState FakeHAL_GetOutput(GPIO_TypeDef *port, uint16_t pin);
void FakeHAL_Reset(void);
void FakeHAL_SetTick(uint32_t tick);
void FakeHAL_AdvanceTick(uint32_t delta_ms);
void FakeHAL_SetPowerGood(GPIO_PinState state);
GPIO_PinState FakeHAL_GetSysOn(void);

void FakeHAL_ResetSensors(void);
extern ADC_HandleTypeDef fake_adc;
struct Sensors_AcquisitionOps;
const struct Sensors_AcquisitionOps *FakeHAL_SensorOps(void);
void FakeHAL_CompletePair(uint16_t battery_sum, uint16_t tip_sum,
                          uint8_t acquired_channels);
void FakeHAL_QueuePair(uint16_t battery_sum, uint16_t tip_sum);
void FakeHAL_DeliverQueuedPair(void);
void FakeHAL_SetStopReady(bool ready);
void FakeHAL_SetTriggerReady(bool ready);
void FakeHAL_SetArmReady(bool ready);
uint32_t FakeHAL_TriggerCount(void);
uint32_t FakeHAL_StopCount(void);
#endif
