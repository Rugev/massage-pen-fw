/* System power and low-power operation. */
#ifndef USER_POWER_H
#define USER_POWER_H

#define MCU_SUPPLY_MV                           2500U
#define SYS_ON_ACTIVE_HIGH                      1U
#define SYS_PG_ACTIVE_HIGH                      1U
/* Low battery: enter Standby, then request charger battery disconnection. */
#define BATTERY_STANDBY_MV                      3000U
#define BATTERY_DISCONNECT_MV                   2900U

/* Public API and implementation remain undecided. */

#endif /* USER_POWER_H */
