# System power and safety

Use `SYS_ON_Pin` to control power to vibration, heater and analog inputs;
read `SYS_PG_Pin` for system power status.
Standby waits in low power for `BUTTON_PWR_ON_Pin` or `CHRG_INT_Pin` wake.
Charging and Normal operation enable `SYS_ON`.

Definitions are in [main.h](../../Core/Inc/main.h); configuration is in the
read-only [.ioc](../../Massage_pen_FW.ioc) and generated code.
One or two safety watchdogs are intended; selection is undecided.
Power sequencing, low-power implementation and safety design will be decided later.

`SYS_ON` low disables the vibration motor, battery sensing resistors, heater
and temperature feedback. Operate only while `SYS_PG` is high; keep checking
it during operation. Shut down the system at the battery threshold in
`User/Inc/power.h`, which also records MCU supply voltage and signal polarities.
