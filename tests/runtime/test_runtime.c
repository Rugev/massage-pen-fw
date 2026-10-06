#include <assert.h>
#include <stdio.h>
#include "runtime.h"
#include "firmware_hw.h"
#include "fake_hal.h"
#include "main.h"
#include "buttons.h"
static App_Snapshot snapshot;
static unsigned init_count, update_count, wake_count, interrupt_count, bind_count;
static App_WakeSource wake_source;
static bool verified;
void FirmwareHW_Init(App_Bindings *b, ADC_HandleTypeDef *a, I2C_HandleTypeDef *c,
                     I2C_HandleTypeDef *m, TIM_HandleTypeDef *t)
{ (void)b; (void)a; (void)c; (void)m; (void)t; ++bind_count; }
void App_Init(const App_Bindings *b, uint32_t now)
{ (void)b; (void)now; ++init_count; snapshot = (App_Snapshot){.state=APP_STARTUP}; }
void App_Update(uint32_t now, bool available)
{ (void)now; assert(available); ++update_count; }
App_Snapshot App_GetSnapshot(void) { return snapshot; }
void App_Wake(uint32_t now, App_WakeSource s, bool v, uint32_t started)
{ (void)now; (void)started; ++wake_count; wake_source=s; verified=v;
  snapshot = (App_Snapshot){.state = snapshot.state == APP_FAULT_SLEEP ? APP_FAULT_DISPLAY : APP_WAKEUP}; }
void Charging_OnInterrupt(void) { ++interrupt_count; }
int main(void)
{
    FakeHAL_Reset();
    FakeHAL_SetInput(CHRG_INT_GPIO_Port, CHRG_INT_Pin, GPIO_PIN_SET);
    Runtime_Poll(); assert(update_count == 0U && interrupt_count == 0U);
    Runtime_Init(NULL,NULL,NULL,NULL,NULL);
    Runtime_Init(NULL,NULL,NULL,NULL,NULL);
    assert(init_count == 1U && bind_count == 1U);
    Runtime_Poll(); assert(update_count == 0U);
    FakeHAL_AdvanceTick(1U); Runtime_Poll(); Runtime_Poll(); assert(update_count==1U);
    FakeHAL_AdvanceTick(8U); Runtime_Poll(); assert(update_count==2U);
    assert(Runtime_GetObservation().missed_ticks==7U);
    /* Continue drains, held shutdown input cannot wake. */
    FakeHAL_SetInput(BUTTON_PWR_ON_GPIO_Port, BUTTON_PWR_ON_Pin, GPIO_PIN_SET);
    Runtime_Poll(); /* shutdown hold edge was seen during active operation */
    snapshot.sleep_requested=true; snapshot.shutdown_pending=true;
    FakeHAL_AdvanceTick(1U); Runtime_Poll(); assert(update_count==3U);
    snapshot.sleep_ready=true; snapshot.shutdown_pending=false; snapshot.state=APP_SLEEP;
    FakeHAL_AdvanceTick(1U); Runtime_Poll(); assert(Runtime_GetObservation().polling_sleep);
    FakeHAL_AdvanceTick(30U); Runtime_Poll(); assert(wake_count==0U && update_count==4U);
    FakeHAL_SetInput(BUTTON_PWR_ON_GPIO_Port, BUTTON_PWR_ON_Pin, GPIO_PIN_RESET); Runtime_Poll();
    FakeHAL_SetInput(BUTTON_PWR_ON_GPIO_Port, BUTTON_PWR_ON_Pin, GPIO_PIN_SET); Runtime_Poll();
    assert(wake_count==1U && wake_source==APP_WAKE_POWER && !verified && init_count==1U);
    assert(!Runtime_GetObservation().polling_sleep);
    /* INT polled within same tick, one episode; also wakes fault sleep. */
    FakeHAL_SetInput(CHRG_INT_GPIO_Port, CHRG_INT_Pin, GPIO_PIN_RESET); Runtime_Poll(); Runtime_Poll();
    assert(interrupt_count==1U);
    FakeHAL_SetInput(CHRG_INT_GPIO_Port, CHRG_INT_Pin, GPIO_PIN_SET); Runtime_Poll();
    snapshot=(App_Snapshot){.state=APP_FAULT_SLEEP,.sleep_requested=true,.sleep_ready=true};
    FakeHAL_AdvanceTick(1U); Runtime_Poll();
    FakeHAL_SetInput(CHRG_INT_GPIO_Port, CHRG_INT_Pin, GPIO_PIN_RESET); Runtime_Poll();
    assert(wake_count==2U && wake_source==APP_WAKE_CHARGER && snapshot.state==APP_FAULT_DISPLAY);
    assert(interrupt_count==2U && init_count==1U);
    FakeHAL_SetTick(UINT32_MAX); Runtime_Poll(); unsigned before=update_count;
    FakeHAL_AdvanceTick(1U); Runtime_Poll(); assert(update_count==before+1U);
    snapshot=(App_Snapshot){.state=APP_SLEEP,.sleep_requested=true,.shutdown_pending=true};
    FakeHAL_SetInput(BUTTON_PWR_ON_GPIO_Port, BUTTON_PWR_ON_Pin, GPIO_PIN_RESET); Runtime_Poll();
    FakeHAL_SetInput(BUTTON_PWR_ON_GPIO_Port, BUTTON_PWR_ON_Pin, GPIO_PIN_SET); Runtime_Poll();
    unsigned wakes_before=wake_count;
    FakeHAL_AdvanceTick(1U); Runtime_Poll(); assert(wake_count==wakes_before);
    snapshot.sleep_ready=true; snapshot.shutdown_pending=false;
    FakeHAL_AdvanceTick(1U); Runtime_Poll(); Runtime_Poll();
    assert(wake_count==wakes_before+1U && !Runtime_GetObservation().polling_sleep);
    Runtime_Poll(); assert(wake_count==wakes_before+1U);
    /* Charger edge on the sleep-entry tick also survives. */
    FakeHAL_SetInput(CHRG_INT_GPIO_Port, CHRG_INT_Pin, GPIO_PIN_SET); Runtime_Poll();
    snapshot=(App_Snapshot){.state=APP_SLEEP,.sleep_requested=true,.sleep_ready=true};
    FakeHAL_AdvanceTick(1U); FakeHAL_SetInput(CHRG_INT_GPIO_Port,CHRG_INT_Pin,GPIO_PIN_RESET);
    Runtime_Poll(); Runtime_Poll();
    assert(wake_count==wakes_before+2U && wake_source==APP_WAKE_CHARGER);
    FakeHAL_SetInput(CHRG_INT_GPIO_Port,CHRG_INT_Pin,GPIO_PIN_SET);
    FakeHAL_SetInput(BUTTON_PWR_ON_GPIO_Port,BUTTON_PWR_ON_Pin,GPIO_PIN_RESET); Runtime_Poll();
    snapshot=(App_Snapshot){.state=APP_SLEEP,.sleep_requested=true,.sleep_ready=true};
    FakeHAL_AdvanceTick(1U); Runtime_Poll();
    FakeHAL_SetInput(CHRG_INT_GPIO_Port,CHRG_INT_Pin,GPIO_PIN_RESET);
    FakeHAL_SetInput(BUTTON_PWR_ON_GPIO_Port,BUTTON_PWR_ON_Pin,GPIO_PIN_SET); Runtime_Poll();
    assert(wake_source==APP_WAKE_POWER && !verified);
    puts("runtime dispatcher tests passed");
}
