#include "mp2724.h"
#include <stddef.h>

/* Register metadata only: values/encodings and field notes live in mp2724.h.
 * Skip the undocumented gap; do not use array index as a register address.
 * No writes are issued and no factory default is applied to the board.
 */
#define MP2724_CONFIG_INFO(name) { \
    MP2724_REG_##name, MP2724_##name##_RESET_DEFAULT, UINT8_MAX, \
    MP2724_##name##_WRITABLE_MASK, MP2724_##name##_COMMAND_MASK \
}
#define MP2724_STATUS_INFO(name) { \
    MP2724_REG_##name, 0U, 0U, \
    MP2724_##name##_WRITABLE_MASK, MP2724_##name##_COMMAND_MASK \
}

const mp2724_register_info_t mp2724_registers[MP2724_REGISTER_COUNT] = {
    MP2724_CONFIG_INFO(CHG_CTRL0),
    MP2724_CONFIG_INFO(IIN),
    MP2724_CONFIG_INFO(CHG_PARAMETER0),
    MP2724_CONFIG_INFO(CHG_PARAMETER1),
    MP2724_CONFIG_INFO(CHG_PARAMETER2),
    MP2724_CONFIG_INFO(CHG_PARAMETER3),
    MP2724_CONFIG_INFO(CHG_CTRL1),
    MP2724_CONFIG_INFO(CHG_CTRL2),
    MP2724_CONFIG_INFO(CHG_CTRL3),
    MP2724_CONFIG_INFO(CHG_CTRL4),
    MP2724_CONFIG_INFO(VIN_DET),
    MP2724_CONFIG_INFO(CHG_CTRL5),
    MP2724_CONFIG_INFO(NTC_ACTION),
    MP2724_CONFIG_INFO(NTC_TH),
    MP2724_CONFIG_INFO(VIN_IMPD),
    MP2724_CONFIG_INFO(INT_MASK),
    MP2724_STATUS_INFO(STATUS0),
    MP2724_STATUS_INFO(STATUS1),
    MP2724_STATUS_INFO(STATUS2),
    MP2724_STATUS_INFO(STATUS3),
    MP2724_STATUS_INFO(STATUS4),
    MP2724_STATUS_INFO(STATUS5),
};

#undef MP2724_CONFIG_INFO
#undef MP2724_STATUS_INFO

static I2C_Device device;
static const mp2724_register_info_t *find_register(uint8_t reg)
{
    for (uint32_t i=0; i<MP2724_REGISTER_COUNT; ++i)
        if (mp2724_registers[i].address==reg) return &mp2724_registers[i];
    return NULL;
}
void MP2724_Init(I2C_HandleTypeDef *h,const I2C_DeviceOps *ops)
{ I2C_DeviceInit(&device,h,ops,MP2724_I2C_ADDRESS_7BIT<<1); }
bool MP2724_RequestRead(uint8_t reg)
{ return find_register(reg)!=NULL && I2C_DeviceRequest(&device,reg,0,0,0,0,false); }
static uint8_t action_mask(const mp2724_register_info_t *info)
{ return info->command_mask | (info->address==MP2724_REG_CHG_CTRL3 ? MP2724_BATTFET_DIS_MASK : 0U); }
static bool request_config(uint8_t reg,uint8_t mask,uint8_t value,bool idle)
{
    const mp2724_register_info_t *info=find_register(reg);
    /* Detection and battery-discharge OCP can update these whole bytes.
     * Even unrelated host fields require exclusive idle hardware ownership. */
    if (!idle && (reg==MP2724_REG_IIN || reg==MP2724_REG_CHG_CTRL3)) return false;
    if (info==NULL || mask==0 || (mask & ~info->writable_mask)!=0 ||
        (mask & action_mask(info))!=0 || (value & ~mask)!=0) return false;
    return I2C_DeviceRequest(&device,reg,mask,value,action_mask(info),
                             reg==MP2724_REG_CHG_CTRL3 ? MP2724_BATTFET_DIS_MASK : 0U,false);
}
bool MP2724_RequestConfig(uint8_t reg,uint8_t mask,uint8_t value)
{ return request_config(reg,mask,value,false); }
bool MP2724_RequestConfigIdle(uint8_t reg,uint8_t mask,uint8_t value)
{ return request_config(reg,mask,value,true); }
bool MP2724_RequestCommand(uint8_t reg,uint8_t mask,uint8_t value)
{
    const mp2724_register_info_t *info=find_register(reg);
    if (info==NULL || mask==0 || (mask & ~action_mask(info))!=0 || (value & ~mask)!=0) return false;
    return I2C_DeviceRequest(&device,reg,mask,value,action_mask(info),0,true);
}
void MP2724_Update(void) { I2C_DeviceUpdate(&device); }
I2C_DeviceResult MP2724_GetResult(void) { return device.result; }
bool MP2724_Acknowledge(void) { return I2C_DeviceAcknowledge(&device); }
void MP2724_Cancel(void) { I2C_DeviceCancel(&device); }
void MP2724_OnReadComplete(I2C_HandleTypeDef *h) { I2C_DeviceOnComplete(&device,h,false); }
void MP2724_OnWriteComplete(I2C_HandleTypeDef *h) { I2C_DeviceOnComplete(&device,h,true); }
void MP2724_OnError(I2C_HandleTypeDef *h) { I2C_DeviceOnError(&device,h); }
