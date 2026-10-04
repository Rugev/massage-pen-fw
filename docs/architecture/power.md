# System power and safety

Use `SYS_ON_Pin` to control power to vibration, heater and analog inputs;
read `SYS_PG_Pin` for system power status.
Standby waits in low power for `BUTTON_PWR_ON_Pin` or `CHRG_INT_Pin` wake.
Charging and Normal operation enable `SYS_ON`.

Definitions are in [main.h](../../Massage_pen_FW/Core/Inc/main.h); configuration is in the
read-only [.ioc](../../Massage_pen_FW/Massage_pen_FW.ioc) and generated code.
One or two safety watchdogs are intended; selection is undecided.
Power sequencing, low-power implementation and safety design will be decided later.

`SYS_ON` low disables the vibration motor, battery sensing resistors, heater
and temperature feedback. Operate only while `SYS_PG` is high; keep checking
it during operation. Enter Standby at `BATTERY_STANDBY_MV`; at the lower
`BATTERY_DISCONNECT_MV`, request complete battery disconnection through the charger.
Both thresholds are in `Massage_pen_FW/User/Inc/power.h`, along with MCU supply and polarities.
Voltage monitoring in Standby remains to be designed because `SYS_ON` disables
the battery sensing resistors.
