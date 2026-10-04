# Charging

Communicate with the MP2724GRH battery charger over I2C and read analog
battery voltage. Charging mode enables `SYS_ON`, monitors the charger and
battery, and displays charge on LEDs. Hybrid mode also operates heat/vibration.
The charger interrupt is a wake source in Standby.

Use `CHRG_I2C_SCL_Pin`, `CHRG_I2C_SDA_Pin`, `CHRG_INT_Pin` and `VBATT_SENS_Pin`
from [main.h](../../Massage_pen_FW/Core/Inc/main.h). Consult the read-only
[.ioc](../../Massage_pen_FW/Massage_pen_FW.ioc) for configuration.
Skeletons: `Massage_pen_FW/User/Src/charging.c` and separate driver `Massage_pen_FW/User/Src/mp2724.c`,
with matching headers in `Massage_pen_FW/User/Inc`. APIs and charging policy remain undecided.

Battery: 1S Li-ion. Voltage range, nominal voltage and maximum charging current
are configurable in `Massage_pen_FW/User/Inc/charging.h`; address is in `Massage_pen_FW/User/Inc/mp2724.h`.
Battery voltage is twice the ADC input voltage; the configurable multiplier
is in `Massage_pen_FW/User/Inc/sensors.h`.

Request complete battery disconnection through the charger at
`BATTERY_DISCONNECT_MV` in `Massage_pen_FW/User/Inc/power.h`; the higher `BATTERY_STANDBY_MV`
requests Standby only. The disconnect command remains to be implemented.
