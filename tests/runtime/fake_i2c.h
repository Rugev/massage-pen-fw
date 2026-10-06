#ifndef FAKE_I2C_H
#define FAKE_I2C_H
#include "i2c_device.h"
extern I2C_HandleTypeDef fake_charger_i2c, fake_driver_i2c;
void FakeI2C_Reset(void);
uint32_t FakeI2C_ChargeEnableWrites(void);
const I2C_DeviceOps *FakeI2C_Ops(void);
bool FakeI2C_HasPending(I2C_HandleTypeDef *h);
void FakeI2C_Complete(I2C_HandleTypeDef *h);
void FakeI2C_Error(I2C_HandleTypeDef *h);
void FakeI2C_Set(uint8_t address, uint8_t reg, uint8_t value);
uint8_t FakeI2C_Get(uint8_t address, uint8_t reg);
void FakeI2C_SetDrainReady(bool ready);
void FakeI2C_QueueOldCompletion(I2C_HandleTypeDef *h);
uint32_t FakeI2C_Starts(I2C_HandleTypeDef *h);
uint32_t FakeI2C_Writes(I2C_HandleTypeDef *h);
#endif
