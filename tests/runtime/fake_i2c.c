#include "fake_i2c.h"
#include "mp2724.h"
#include "drv2624.h"
#include <assert.h>
#include <string.h>
I2C_HandleTypeDef fake_charger_i2c, fake_driver_i2c;
typedef struct { uint8_t regs[256], *buffer, reg; bool pending, write, queued; uint32_t starts, writes; } Bus;
static Bus buses[2];
static bool drain_ready;
static uint32_t charge_enable_writes;
static Bus *bus(I2C_HandleTypeDef *h) { assert(h == &fake_charger_i2c || h == &fake_driver_i2c); return &buses[h == &fake_driver_i2c]; }
static void callback(I2C_HandleTypeDef *h, bool write) {
    if (h == &fake_charger_i2c) { if(write) MP2724_OnWriteComplete(h); else MP2724_OnReadComplete(h); }
    else { if(write) DRV2624_OnWriteComplete(h); else DRV2624_OnReadComplete(h); }
}
static bool start(I2C_HandleTypeDef *h, uint16_t address, uint8_t reg, uint8_t *buffer, bool write) {
    Bus *b=bus(h); assert(!b->pending); assert(address == (h == &fake_charger_i2c ? 0x7eU : 0xb4U));
    b->reg=reg; b->buffer=buffer; b->write=write; b->pending=true; b->starts++; if(write) b->writes++; return true;
}
static bool read_it(I2C_HandleTypeDef *h, uint16_t a, uint8_t r, uint8_t *v) {return start(h,a,r,v,false);}
static bool write_it(I2C_HandleTypeDef *h, uint16_t a, uint8_t r, uint8_t *v) {return start(h,a,r,v,true);}
static bool stop(I2C_HandleTypeDef *h) {
    Bus *b=bus(h); if(b->queued) {b->queued=false; callback(h,b->write);} /* Old ISR must be ignored while draining. */
    if(!drain_ready) return false;
    b->pending=false; b->buffer=NULL; return true;
}
static const I2C_DeviceOps ops={read_it,write_it,stop};
const I2C_DeviceOps *FakeI2C_Ops(void) {return &ops;}
void FakeI2C_Reset(void) {memset(buses,0,sizeof buses); charge_enable_writes=0U; drain_ready=true; FakeHAL_Reset();}
bool FakeI2C_HasPending(I2C_HandleTypeDef *h) {return bus(h)->pending;}
void FakeI2C_Complete(I2C_HandleTypeDef *h) {
    Bus *b=bus(h); assert(b->pending); bool write=b->write;
    if(write) {
        if(h == &fake_charger_i2c && b->reg == MP2724_REG_CHG_CTRL4 &&
           (*b->buffer & MP2724_EN_CHG_MASK)) charge_enable_writes++;
        b->regs[b->reg]=*b->buffer;
        if(h == &fake_charger_i2c && b->reg == 0) b->regs[0]&=0x7f;
        if(h == &fake_charger_i2c && b->reg == 7) b->regs[7]&=0xbf;
        if(h == &fake_driver_i2c && b->reg == 12) b->regs[12]&=0xfe; /* Fast completed process. */
    } else { *b->buffer=b->regs[b->reg]; if(h == &fake_driver_i2c && b->reg == 1) b->regs[1]&=0x60; }
    b->pending=false; callback(h,write);
}
void FakeI2C_Error(I2C_HandleTypeDef *h) {assert(bus(h)->pending); if(h==&fake_charger_i2c) MP2724_OnError(h); else DRV2624_OnError(h);}
void FakeI2C_Set(uint8_t a,uint8_t r,uint8_t v) {buses[a==0x5a].regs[r]=v;}
uint8_t FakeI2C_Get(uint8_t a,uint8_t r) {return buses[a==0x5a].regs[r];}
void FakeI2C_SetDrainReady(bool r) {drain_ready=r;}
void FakeI2C_QueueOldCompletion(I2C_HandleTypeDef *h) {bus(h)->queued=true;}
uint32_t FakeI2C_Starts(I2C_HandleTypeDef *h) {return bus(h)->starts;}
uint32_t FakeI2C_Writes(I2C_HandleTypeDef *h) {return bus(h)->writes;}

uint32_t FakeI2C_ChargeEnableWrites(void) {return charge_enable_writes;}
