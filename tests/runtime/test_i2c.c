#include "fake_i2c.h"
#include "mp2724.h"
#include "drv2624.h"
#include <assert.h>
#include <stdio.h>
static void reset(void) {FakeI2C_Reset(); MP2724_Init(&fake_charger_i2c,FakeI2C_Ops()); DRV2624_Init(&fake_driver_i2c,FakeI2C_Ops());}
static void mc(void) {FakeI2C_Complete(&fake_charger_i2c); MP2724_Update();}
static void dc(void) {FakeI2C_Complete(&fake_driver_i2c); DRV2624_Update();}
static void delay(uint32_t ms) {FakeHAL_AdvanceTick(ms); MP2724_Update(); DRV2624_Update();}
/* Catches early timeout, early retries, failure double counting and ownership reuse. */
static void timeout_and_drain(void) {
    reset(); assert(MP2724_RequestRead(0x12)); MP2724_Update();
    delay(4); assert(MP2724_GetResult().failures==0); FakeI2C_SetDrainReady(false);
    FakeI2C_QueueOldCompletion(&fake_charger_i2c); delay(1);
    assert(MP2724_GetResult().failures==1); delay(8);
    assert(FakeI2C_Starts(&fake_charger_i2c)==1); assert(MP2724_GetResult().state==I2C_RESULT_PENDING);
    FakeI2C_SetDrainReady(true); MP2724_Update();
    assert(FakeI2C_Starts(&fake_charger_i2c)==2); mc(); assert(MP2724_GetResult().state==I2C_RESULT_READ);
    assert(!MP2724_RequestRead(0x12)); assert(MP2724_Acknowledge());
    assert(MP2724_RequestRead(0x12)); MP2724_Update(); delay(5);
    uint32_t starts=FakeI2C_Starts(&fake_charger_i2c); delay(4); assert(FakeI2C_Starts(&fake_charger_i2c)==starts);
    delay(1); assert(FakeI2C_Starts(&fake_charger_i2c)==starts+1);
}
/* Catches write/readback failure being reset by prerequisite reads or other driver success. */
static void configuration_retry_failures(void) {
    reset(); assert(MP2724_RequestConfig(9,4,4)); MP2724_Update(); mc();
    FakeI2C_Error(&fake_charger_i2c); MP2724_Update(); assert(MP2724_GetResult().failures==1);
    assert(DRV2624_RequestRead(0)); DRV2624_Update(); dc(); assert(MP2724_GetResult().failures==1);
    delay(5); mc(); mc(); FakeI2C_Error(&fake_charger_i2c); MP2724_Update(); assert(MP2724_GetResult().failures==2);
    delay(5); mc(); mc(); FakeI2C_Set(0x3f,9,0); mc();
    assert(MP2724_GetResult().state==I2C_RESULT_FAILED); assert(MP2724_GetResult().failures==3);
    assert(MP2724_GetResult().exhausted); assert(MP2724_Acknowledge()); assert(!MP2724_RequestRead(0x12));
}
static void matching_fields_and_reserved(void) {
    reset(); FakeI2C_Set(0x3f,9,0x88); assert(MP2724_RequestConfig(9,4,4)); MP2724_Update(); mc(); mc();
    assert(FakeI2C_Get(0x3f,9)==0x8c); assert(MP2724_GetResult().state==I2C_RESULT_PENDING);
    FakeI2C_Set(0x3f,9,0xec); mc(); assert(MP2724_GetResult().state==I2C_RESULT_VERIFIED);
    assert(MP2724_Acknowledge()); FakeI2C_Set(0x3f,7,0x40); assert(MP2724_RequestConfig(7,0x30,0x20)); MP2724_Update(); mc(); mc();
    assert(FakeI2C_Get(0x3f,7)==0x20); mc(); assert(MP2724_GetResult().state==I2C_RESULT_VERIFIED);
    FakeI2C_Set(0x5a,8,3); assert(DRV2624_RequestConfig(8,0x40,0x40)); DRV2624_Update(); dc(); dc(); dc();
    assert(FakeI2C_Get(0x5a,8)==0x43); assert(DRV2624_GetResult().state==I2C_RESULT_VERIFIED);
    assert(DRV2624_Acknowledge()); FakeI2C_Set(0x5a,0x23,1); assert(DRV2624_RequestConfigIdle(0x23,0x70,0x20)); DRV2624_Update(); dc(); dc();
    FakeI2C_Set(0x5a,0x23,0x23); dc(); assert(DRV2624_GetResult().state==I2C_RESULT_VERIFIED);
}
static void read_failure_limit_and_rollover(void) {
    reset(); FakeHAL_SetTick(UINT32_MAX-2); assert(MP2724_RequestRead(0x12)); MP2724_Update();
    delay(5); assert(MP2724_GetResult().failures==1); delay(5); delay(5); assert(MP2724_GetResult().failures==2); delay(5); delay(5);
    assert(MP2724_GetResult().exhausted); assert(FakeI2C_Starts(&fake_charger_i2c)==3);
}
static void command_semantics(void) {
    reset(); assert(!MP2724_RequestConfig(0,0x80,0x80)); assert(!MP2724_RequestConfig(8,0x80,0x80)); assert(!DRV2624_RequestConfig(12,1,1));
    assert(DRV2624_RequestCommand(12,1,1)); DRV2624_Update(); dc(); dc(); dc();
    assert(DRV2624_GetResult().state==I2C_RESULT_COMMAND_ACCEPTED); assert(DRV2624_GetResult().value==0); assert(FakeI2C_Writes(&fake_driver_i2c)==1);
    assert(DRV2624_Acknowledge()); assert(DRV2624_RequestCommand(12,1,1)); DRV2624_Update(); dc();
    FakeI2C_Error(&fake_driver_i2c); DRV2624_Update(); delay(30);
    assert(DRV2624_GetResult().state==I2C_RESULT_COMMAND_UNCERTAIN); assert(FakeI2C_Writes(&fake_driver_i2c)==2);
    assert(DRV2624_Acknowledge()); assert(DRV2624_RequestCommand(12,1,1)); DRV2624_Update(); dc(); dc();
    FakeI2C_Error(&fake_driver_i2c); DRV2624_Update(); delay(5); dc();
    assert(DRV2624_GetResult().state==I2C_RESULT_COMMAND_ACCEPTED); assert(FakeI2C_Writes(&fake_driver_i2c)==3);
}
static void status_snapshot_and_validation(void) {
    reset(); assert(!DRV2624_RequestRead(1)); assert(!MP2724_RequestRead(0x0b)); assert(!DRV2624_RequestRead(0x31));
    assert(!DRV2624_RequestConfig(0xff,0xff,3)); assert(!MP2724_RequestConfig(9,0x80,0x80));
    assert(!MP2724_RequestConfig(9,4,8)); assert(!MP2724_RequestConfig(9,0,0));
    FakeI2C_Set(0x5a,1,0x8b); assert(DRV2624_RequestStatus()); DRV2624_Update(); dc();
    DRV2624_StatusSnapshot a=DRV2624_GetStatus(), b=DRV2624_GetStatus();
    assert(a.valid && a.value==0x8b && a.sequence==1); assert(b.value==a.value && b.sequence==a.sequence); assert(FakeI2C_Get(0x5a,1)==0);
    assert(DRV2624_Acknowledge()); assert(DRV2624_RequestRead(0)); DRV2624_Update(); dc(); assert(DRV2624_GetStatus().value==0x8b);
    assert(DRV2624_Acknowledge()); assert(DRV2624_RequestStatus()); DRV2624_Update(); dc(); assert(DRV2624_GetStatus().sequence==2);
    assert(DRV2624_GetStatus().value==0x8b);
    assert(!DRV2624_AcknowledgeStatus(1)); assert(DRV2624_AcknowledgeStatus(2));
    assert(DRV2624_GetStatus().value==0);
    MP2724_Init(&fake_charger_i2c,NULL); assert(!MP2724_RequestRead(0x12));
}
static void cancellation(void) {
    reset(); assert(MP2724_RequestRead(0x12)); MP2724_Update(); FakeI2C_SetDrainReady(false); MP2724_Cancel();
    FakeI2C_QueueOldCompletion(&fake_charger_i2c); delay(10); assert(!MP2724_Acknowledge()); assert(!MP2724_RequestRead(0x12));
    FakeI2C_SetDrainReady(true); MP2724_Update(); assert(MP2724_GetResult().state==I2C_RESULT_CANCELLED); assert(MP2724_Acknowledge());
    assert(MP2724_RequestRead(0x12)); MP2724_Update(); mc(); assert(MP2724_GetResult().state==I2C_RESULT_READ);
}
/* Catches wrong handle/type callbacks completing an operation, and retries of
 * an ACKed command whose observation fails repeatedly. */
static void callback_and_command_boundaries(void) {
    reset(); FakeI2C_Set(0x3f,0x12,0x5a); assert(MP2724_RequestRead(0x12)); MP2724_Update();
    MP2724_OnReadComplete(&fake_driver_i2c); MP2724_OnWriteComplete(&fake_charger_i2c); MP2724_Update();
    assert(MP2724_GetResult().state==I2C_RESULT_PENDING); mc(); assert(MP2724_GetResult().value==0x5a);
    assert(MP2724_Acknowledge()); FakeI2C_Set(0x3f,0,0x7f); assert(MP2724_RequestCommand(0,0x80,0x80)); MP2724_Update(); mc(); mc();
    FakeI2C_Set(0x3f,0,0x0b); mc(); assert(MP2724_GetResult().state==I2C_RESULT_COMMAND_ACCEPTED);
    assert(MP2724_Acknowledge()); assert(MP2724_RequestCommand(8,0x80,0x80)); MP2724_Update(); mc();
    FakeI2C_Error(&fake_charger_i2c); MP2724_Update(); delay(20);
    assert(MP2724_GetResult().state==I2C_RESULT_COMMAND_UNCERTAIN); assert(FakeI2C_Writes(&fake_charger_i2c)==2);
    reset(); assert(DRV2624_RequestCommand(12,1,1)); DRV2624_Update(); dc(); dc();
    for (unsigned i=0; i<3; ++i) {
        FakeI2C_Error(&fake_driver_i2c); DRV2624_Update();
        if(i<2) delay(5);
    }
    assert(DRV2624_GetResult().exhausted); assert(DRV2624_GetResult().failures==3);
    assert(FakeI2C_Writes(&fake_driver_i2c)==1);
}
/* Clearing or replaying an observed shipping/OCP bit through ordinary field
 * configuration would change the battery power path without a command. */
static void shipping_state_guard(void) {
    reset(); FakeI2C_Set(0x3f,8,0xff); assert(MP2724_RequestConfigIdle(8,7,4)); MP2724_Update(); mc();
    assert(MP2724_GetResult().state==I2C_RESULT_REJECTED);
    assert(FakeI2C_Writes(&fake_charger_i2c)==0); assert(FakeI2C_Get(0x3f,8)==0xff);
    assert(MP2724_Acknowledge()); assert(MP2724_RequestCommand(8,0x80,0)); MP2724_Update(); mc(); mc(); mc();
    assert(MP2724_GetResult().state==I2C_RESULT_COMMAND_ACCEPTED); assert(FakeI2C_Get(0x3f,8)==0x7f);
}
/* A wedged adapter must report failed recovery without ever releasing its
 * buffer, and repeated ticks or late callbacks must not reopen ownership. */
static void bounded_recovery(void) {
    reset(); assert(MP2724_RequestRead(0x12)); MP2724_Update();
    FakeI2C_SetDrainReady(false); delay(5); delay(19);
    assert(!MP2724_GetResult().exhausted); delay(1);
    assert(MP2724_GetResult().exhausted); assert(MP2724_GetResult().state==I2C_RESULT_FAILED);
    assert(MP2724_GetResult().failures==1); assert(!MP2724_Acknowledge());
    MP2724_Update(); MP2724_Update(); assert(MP2724_GetResult().failures==1);
    FakeI2C_QueueOldCompletion(&fake_charger_i2c); delay(100);
    assert(MP2724_GetResult().state==I2C_RESULT_FAILED); assert(FakeI2C_Starts(&fake_charger_i2c)==1);
    FakeI2C_SetDrainReady(true); MP2724_Update(); assert(MP2724_Acknowledge()); assert(!MP2724_RequestRead(0x12));
    reset(); assert(DRV2624_RequestCommand(12,1,1)); DRV2624_Update(); dc();
    FakeI2C_SetDrainReady(false); FakeI2C_Error(&fake_driver_i2c); DRV2624_Update();
    assert(DRV2624_GetResult().state==I2C_RESULT_COMMAND_UNCERTAIN); assert(!DRV2624_Acknowledge());
    delay(20); assert(DRV2624_GetResult().exhausted); assert(DRV2624_GetResult().state==I2C_RESULT_COMMAND_UNCERTAIN);
    FakeI2C_QueueOldCompletion(&fake_driver_i2c); delay(100); assert(FakeI2C_Writes(&fake_driver_i2c)==1);
    FakeI2C_SetDrainReady(true); DRV2624_Update(); assert(DRV2624_Acknowledge()); assert(!DRV2624_RequestCommand(12,1,1));
    reset(); assert(DRV2624_RequestCommand(12,1,1)); DRV2624_Update();
    FakeI2C_Error(&fake_driver_i2c); DRV2624_Update(); delay(5);
    FakeI2C_Error(&fake_driver_i2c); DRV2624_Update(); delay(5); dc();
    FakeI2C_SetDrainReady(false); FakeI2C_Error(&fake_driver_i2c); DRV2624_Update();
    assert(DRV2624_GetResult().exhausted);
    assert(DRV2624_GetResult().state==I2C_RESULT_COMMAND_UNCERTAIN);
    assert(!DRV2624_Acknowledge()); assert(FakeI2C_Writes(&fake_driver_i2c)==1);
}
/* Reject even unrelated fields: the full register write could replace a BEMF
 * gain changed autonomously after its prerequisite read. Only an explicit
 * idle lease permits these transactions, including charger detection IIN. */
static void hardware_idle_ownership(void) {
    reset(); FakeI2C_Set(0x5a,0x23,3);
    assert(!DRV2624_RequestConfig(0x23,0xc0,0x80));
    assert(!DRV2624_RequestConfig(0x21,0xff,0x12)); assert(!DRV2624_RequestConfig(0x22,0xff,0x34));
    assert(!MP2724_RequestConfig(1,0x1f,4)); assert(FakeI2C_Starts(&fake_driver_i2c)==0);
    assert(DRV2624_RequestConfigIdle(0x23,0xc0,0x80)); DRV2624_Update(); dc(); dc(); dc();
    assert(DRV2624_GetResult().state==I2C_RESULT_VERIFIED); assert(FakeI2C_Get(0x5a,0x23)==0x83);
    assert(DRV2624_Acknowledge()); assert(MP2724_RequestConfigIdle(1,0x1f,4)); MP2724_Update(); mc(); mc(); mc();
    assert(MP2724_GetResult().state==I2C_RESULT_VERIFIED); assert(FakeI2C_Get(0x3f,1)==4);
}
/* Ordinary configuration cannot acquire whole-register ownership over
 * discharge/OCP: refuse before preread so a later autonomous disconnect cannot
 * be cleared by a stale whole-byte write. Qualified idle lease permits config. */
static void discharge_idle_ownership(void) {
    reset(); FakeI2C_Set(0x3f,8,0x7f);
    assert(!MP2724_RequestConfig(8,7,4));
    FakeI2C_Set(0x3f,8,0xff); MP2724_Update();
    assert(FakeI2C_Starts(&fake_charger_i2c)==0); assert(FakeI2C_Writes(&fake_charger_i2c)==0);
    assert(FakeI2C_Get(0x3f,8)==0xff);
    /* Caller establishes stopped discharge/OCP ownership before using Idle. */
    FakeI2C_Set(0x3f,8,0x7f); assert(MP2724_RequestConfigIdle(8,7,4)); MP2724_Update(); mc();
    FakeI2C_Error(&fake_charger_i2c); MP2724_Update(); delay(5); mc(); mc(); mc();
    assert(MP2724_GetResult().state==I2C_RESULT_VERIFIED); assert(FakeI2C_Get(0x3f,8)==0x7c);
    assert(MP2724_Acknowledge()); assert(!MP2724_RequestConfigIdle(8,0x80,0x80));
    assert(MP2724_RequestCommand(8,0x80,0x80)); MP2724_Update(); mc(); mc(); mc();
    assert(MP2724_GetResult().state==I2C_RESULT_COMMAND_ACCEPTED); assert(FakeI2C_Get(0x3f,8)==0xfc);
}
int main(void) {discharge_idle_ownership(); timeout_and_drain(); configuration_retry_failures(); matching_fields_and_reserved(); read_failure_limit_and_rollover(); command_semantics(); status_snapshot_and_validation(); cancellation(); callback_and_command_boundaries(); shipping_state_guard(); bounded_recovery(); hardware_idle_ownership(); puts("i2c: 12 scenarios passed");}
