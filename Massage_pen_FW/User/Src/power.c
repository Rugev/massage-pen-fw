#include "power.h"
#include "main.h"
#include "buttons.h"
#include "charging.h"

static Power_State_t power_state = POWER_STATE_DISABLED;
static uint32_t power_start_tick;

static GPIO_PinState sys_on_inactive_level(void)
{
    return (SYS_ON_ACTIVE_HIGH != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET;
}

static GPIO_PinState sys_on_active_level(void)
{
    return (SYS_ON_ACTIVE_HIGH != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

static GPIO_PinState sys_pg_active_level(void)
{
    return (SYS_PG_ACTIVE_HIGH != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

static void set_sys_on(int enabled)
{
    HAL_GPIO_WritePin(SYS_ON_GPIO_Port,
                      SYS_ON_Pin,
                      (enabled != 0) ? sys_on_active_level() : sys_on_inactive_level());
}

void Power_Init(void)
{
    power_state = POWER_STATE_DISABLED;
    power_start_tick = 0U;
    set_sys_on(0);
}

void Power_Enable(void)
{
    if ((power_state == POWER_STATE_STARTING) || (power_state == POWER_STATE_READY))
    {
        return;
    }

    set_sys_on(1);
    power_start_tick = HAL_GetTick();
    power_state = POWER_STATE_STARTING;
}

void Power_Disable(void)
{
    set_sys_on(0);
    power_state = POWER_STATE_DISABLED;
}

void Power_Update(void)
{
    GPIO_PinState power_good;

    if ((power_state != POWER_STATE_STARTING) && (power_state != POWER_STATE_READY))
    {
        return;
    }

    if ((power_state == POWER_STATE_STARTING) &&
        ((uint32_t)(HAL_GetTick() - power_start_tick) >= SYS_PG_TIMEOUT_MS))
    {
        power_state = POWER_STATE_FAILURE;
        return;
    }

    power_good = HAL_GPIO_ReadPin(SYS_PG_GPIO_Port, SYS_PG_Pin);
    if (power_good != sys_pg_active_level())
    {
        if (power_state == POWER_STATE_READY)
        {
            power_state = POWER_STATE_FAILURE;
        }
        return;
    }

    power_state = POWER_STATE_READY;
}

Power_State_t Power_GetState(void)
{
    return power_state;
}

static uint8_t wake_levels;
static uint8_t read_wake_levels(void)
{
    uint8_t levels = 0U;
    if (HAL_GPIO_ReadPin(BUTTON_PWR_ON_GPIO_Port, BUTTON_PWR_ON_Pin) ==
        (BUTTONS_ACTIVE_HIGH ? GPIO_PIN_SET : GPIO_PIN_RESET)) levels |= POWER_WAKE_BUTTON;
    if (HAL_GPIO_ReadPin(CHRG_INT_GPIO_Port, CHRG_INT_Pin) ==
        (CHARGER_INTERRUPT_ACTIVE_HIGH ? GPIO_PIN_SET : GPIO_PIN_RESET)) levels |= POWER_WAKE_CHARGER;
    return levels;
}
void Power_InitWakePolling(void) { wake_levels = read_wake_levels(); }
uint8_t Power_PollWake(void)
{
    uint8_t levels = read_wake_levels();
    uint8_t edges = levels & (uint8_t)~wake_levels;
    wake_levels = levels;
    return edges;
}
