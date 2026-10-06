#include <assert.h>
#include <stdio.h>

#include "fake_hal.h"
#include "power.h"

static void reset_power(void)
{
    FakeHAL_Reset();
    Power_Init();
}

static void test_disabled_by_default(void)
{
    reset_power();
    assert(Power_GetState() == POWER_STATE_DISABLED);
    assert(FakeHAL_GetSysOn() == GPIO_PIN_RESET);
}

static void test_waits_for_power_good_and_accepts_it_at_999_ms(void)
{
    reset_power();
    Power_Enable();
    assert(FakeHAL_GetSysOn() == GPIO_PIN_SET);
    assert(Power_GetState() == POWER_STATE_STARTING);

    FakeHAL_AdvanceTick(999U);
    Power_Update();
    assert(Power_GetState() == POWER_STATE_STARTING);

    FakeHAL_SetPowerGood(GPIO_PIN_SET);
    Power_Update();
    assert(Power_GetState() == POWER_STATE_READY);
}

static void test_missing_power_good_fails_at_1000_ms(void)
{
    reset_power();
    Power_Enable();
    FakeHAL_AdvanceTick(1000U);
    Power_Update();
    assert(Power_GetState() == POWER_STATE_FAILURE);
    assert(FakeHAL_GetSysOn() == GPIO_PIN_SET);
}

static void test_power_good_loss_fails_while_enabled(void)
{
    reset_power();
    Power_Enable();
    FakeHAL_SetPowerGood(GPIO_PIN_SET);
    Power_Update();
    assert(Power_GetState() == POWER_STATE_READY);

    FakeHAL_SetPowerGood(GPIO_PIN_RESET);
    Power_Update();
    assert(Power_GetState() == POWER_STATE_FAILURE);
    assert(FakeHAL_GetSysOn() == GPIO_PIN_SET);
}

static void test_intentional_disable_does_not_fault(void)
{
    reset_power();
    Power_Enable();
    Power_Disable();
    Power_Update();
    assert(Power_GetState() == POWER_STATE_DISABLED);
    assert(FakeHAL_GetSysOn() == GPIO_PIN_RESET);
}

static void test_ready_disable_then_power_good_low_does_not_fault(void)
{
    reset_power();
    Power_Enable();
    FakeHAL_SetPowerGood(GPIO_PIN_SET);
    Power_Update();
    assert(Power_GetState() == POWER_STATE_READY);
    Power_Disable();
    FakeHAL_SetPowerGood(GPIO_PIN_RESET);
    Power_Update();
    assert(Power_GetState() == POWER_STATE_DISABLED);
    assert(FakeHAL_GetSysOn() == GPIO_PIN_RESET);
}

static void test_startup_deadline_handles_tick_rollover(void)
{
    reset_power();
    FakeHAL_SetTick(UINT32_MAX - 500U);
    Power_Enable();

    FakeHAL_AdvanceTick(999U);
    Power_Update();
    assert(Power_GetState() == POWER_STATE_STARTING);

    FakeHAL_AdvanceTick(1U);
    Power_Update();
    assert(Power_GetState() == POWER_STATE_FAILURE);
}

int main(void)
{
    test_disabled_by_default();
    test_waits_for_power_good_and_accepts_it_at_999_ms();
    test_missing_power_good_fails_at_1000_ms();
    test_power_good_loss_fails_while_enabled();
    test_intentional_disable_does_not_fault();
    test_ready_disable_then_power_good_low_does_not_fault();
    test_startup_deadline_handles_tick_rollover();
    puts("power tests passed");
    return 0;
}
