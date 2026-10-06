/* System power and low-power operation. */
#ifndef USER_POWER_H
#define USER_POWER_H

#define MCU_SUPPLY_MV                           2500U
#define SYS_ON_ACTIVE_HIGH                      1U
#define SYS_PG_ACTIVE_HIGH                      1U
/* Low battery: enter Standby, then request charger battery disconnection. */
#define BATTERY_STANDBY_MV                      3000U
#define BATTERY_DISCONNECT_MV                   2900U

#include <stdint.h>
#define POWER_WAKE_BUTTON 1U
#define POWER_WAKE_CHARGER 2U
/* Poll raw edges continuously, including active operation. No low-power entry. */
void Power_InitWakePolling(void);
uint8_t Power_PollWake(void);

/* SYS_ON power sequencing. The application owns any fault policy. */
#define SYS_PG_TIMEOUT_MS                       1000U

typedef enum
{
    POWER_STATE_DISABLED = 0,
    POWER_STATE_STARTING,
    POWER_STATE_READY,
    POWER_STATE_FAILURE
} Power_State_t;

void Power_Init(void);
void Power_Enable(void);
void Power_Disable(void);
void Power_Update(void);
Power_State_t Power_GetState(void);

#endif /* USER_POWER_H */
