#include "drv2624.h"
#include <stddef.h>
typedef struct { uint8_t reg, writable; } RegisterInfo;
#define REGISTER(name) {DRV2624_REG_##name, DRV2624_##name##_WRITABLE_MASK}
static const RegisterInfo registers[DRV2624_REGISTER_COUNT] = {
    REGISTER(ID),
    REGISTER(STATUS),
    REGISTER(INTZ_MASK),
    REGISTER(DIAG_Z_RESULT),
    REGISTER(VBAT),
    REGISTER(LRA_PERIOD_H),
    REGISTER(LRA_PERIOD_L),
    REGISTER(MODE),
    REGISTER(CONTROL),
    REGISTER(BATTERY_CONTROL),
    REGISTER(BAT_LIFE_EXT_LVL1),
    REGISTER(BAT_LIFE_EXT_LVL2),
    REGISTER(GO),
    REGISTER(PLAYBACK_CONTROL),
    REGISTER(RTP_INPUT),
    REGISTER(WAV_FRM_SEQ1),
    REGISTER(WAV_FRM_SEQ2),
    REGISTER(WAV_FRM_SEQ3),
    REGISTER(WAV_FRM_SEQ4),
    REGISTER(WAV_FRM_SEQ5),
    REGISTER(WAV_FRM_SEQ6),
    REGISTER(WAV_FRM_SEQ7),
    REGISTER(WAV_FRM_SEQ8),
    REGISTER(WAV_SEQ_LOOP1),
    REGISTER(WAV_SEQ_LOOP2),
    REGISTER(WAV_SEQ_MAIN_LOOP),
    REGISTER(ODT),
    REGISTER(SPT),
    REGISTER(SNT),
    REGISTER(BRT),
    REGISTER(RATED_VOLTAGE),
    REGISTER(OD_CLAMP),
    REGISTER(A_CAL_COMP),
    REGISTER(A_CAL_BEMF),
    REGISTER(FEEDBACK_CONTROL),
    REGISTER(RATED_VOLTAGE_CLAMP),
    REGISTER(OD_CLAMP_LVL1),
    REGISTER(OD_CLAMP_LVL2),
    REGISTER(LRA_DRIVE_CONTROL),
    REGISTER(BEMF_TIMING),
    REGISTER(TIMING_CONTROL),
    REGISTER(AUTO_CAL_TIME),
    REGISTER(LRA_OPEN_LOOP_CONTROL),
    REGISTER(OL_LRA_PERIOD_H),
    REGISTER(OL_LRA_PERIOD_L),
    REGISTER(CURRENT_K),
    REGISTER(RAM_ADDR_H),
    REGISTER(RAM_ADDR_L),
    REGISTER(RAM_DATA),
};
#undef REGISTER
static I2C_Device device;
static DRV2624_StatusSnapshot status;
static bool status_captured;
static const RegisterInfo *find_register(uint8_t reg)
{
    for (uint32_t i=0; i<DRV2624_REGISTER_COUNT; ++i)
        if (registers[i].reg==reg) return &registers[i];
    return NULL;
}
void DRV2624_Init(I2C_HandleTypeDef *h,const I2C_DeviceOps *ops)
{
    I2C_DeviceInit(&device,h,ops,DRV2624_I2C_ADDRESS_7BIT<<1);
    status=(DRV2624_StatusSnapshot){0}; status_captured=false;
}
bool DRV2624_RequestRead(uint8_t reg)
{
    return find_register(reg)!=NULL && reg!=DRV2624_REG_STATUS && reg<DRV2624_REG_RAM_ADDR_H &&
           I2C_DeviceRequest(&device,reg,0,0,0,0,false);
}
bool DRV2624_RequestStatus(void)
{
    if (!I2C_DeviceRequest(&device,DRV2624_REG_STATUS,0,0,0,0,false)) return false;
    status_captured=false; return true;
}
static bool request_config(uint8_t reg,uint8_t mask,uint8_t value,bool idle)
{
    const RegisterInfo *info=find_register(reg);
    /* Calibration owns the entire byte, even when host targets noise/gains. */
    if (!idle && (reg==DRV2624_REG_A_CAL_COMP || reg==DRV2624_REG_A_CAL_BEMF ||
                  reg==DRV2624_REG_FEEDBACK_CONTROL)) return false;
    /* RAM ports advance an internal pointer; ordinary read/write/readback and
     * replay cannot verify them. No RAM transaction API is implemented here. */
    if (info==NULL || mask==0 || reg==DRV2624_REG_GO || reg>=DRV2624_REG_RAM_ADDR_H ||
        (mask & ~info->writable)!=0 || (value & ~mask)!=0) return false;
    return I2C_DeviceRequest(&device,reg,mask,value,0,0,false);
}
bool DRV2624_RequestConfig(uint8_t reg,uint8_t mask,uint8_t value)
{ return request_config(reg,mask,value,false); }
bool DRV2624_RequestConfigIdle(uint8_t reg,uint8_t mask,uint8_t value)
{ return request_config(reg,mask,value,true); }
bool DRV2624_RequestCommand(uint8_t reg,uint8_t mask,uint8_t value)
{
    if (reg!=DRV2624_REG_GO || mask!=DRV2624_GO_MASK || (value & ~mask)!=0) return false;
    return I2C_DeviceRequest(&device,reg,mask,value,DRV2624_GO_MASK,0,true);
}
void DRV2624_Update(void)
{
    I2C_DeviceUpdate(&device);
    if (!status_captured && device.result.reg==DRV2624_REG_STATUS && device.result.state==I2C_RESULT_READ) {
        status.value|=device.result.value; status.sequence++; status.valid=true; status_captured=true;
    }
}
bool DRV2624_AcknowledgeStatus(uint32_t sequence)
{
    if (!status.valid || sequence!=status.sequence) return false;
    status.value=0; return true;
}
I2C_DeviceResult DRV2624_GetResult(void) { return device.result; }
DRV2624_StatusSnapshot DRV2624_GetStatus(void) { return status; }
bool DRV2624_Acknowledge(void) { return I2C_DeviceAcknowledge(&device); }
void DRV2624_Cancel(void) { I2C_DeviceCancel(&device); }
void DRV2624_OnReadComplete(I2C_HandleTypeDef *h) { I2C_DeviceOnComplete(&device,h,false); }
void DRV2624_OnWriteComplete(I2C_HandleTypeDef *h) { I2C_DeviceOnComplete(&device,h,true); }
void DRV2624_OnError(I2C_HandleTypeDef *h) { I2C_DeviceOnError(&device,h); }
