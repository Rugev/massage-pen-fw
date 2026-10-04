# Charging

Communicate with the MP2724GRH battery charger over I2C and read analog
battery voltage. Charging mode enables `SYS_ON`, monitors the charger and
battery, and displays charge on LEDs. Hybrid mode also operates heat/vibration.
The charger interrupt is a wake source in Standby.

Use `CHRG_I2C_SCL_Pin`, `CHRG_I2C_SDA_Pin`, `CHRG_INT_Pin` and `VBATT_SENS_Pin`
from [main.h](../../Core/Inc/main.h). Consult the read-only
[.ioc](../../Massage_pen_FW.ioc) for configuration.
Skeletons: `User/Src/charging.c` and separate driver `User/Src/mp2724.c`,
with matching headers in `User/Inc`. APIs and charging policy remain undecided.

Battery: 1S Li-ion. Voltage range, nominal voltage and maximum charging current
are configurable in `User/Inc/charging.h`; address is in `User/Inc/mp2724.h`.
Battery voltage is twice the ADC input voltage; the configurable multiplier
is in `User/Inc/sensors.h`.
