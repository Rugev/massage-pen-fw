# Vibration

Communicate with the DRV2624 vibration motor controller over I2C.
The system supports vibration during Normal operation and Hybrid mode.

Use `VIBR_I2C_SCL_Pin` and `VIBR_I2C_SDA_Pin` definitions from
[main.h](../../Massage_pen_FW/Core/Inc/main.h). For configuration, consult the read-only
[.ioc](../../Massage_pen_FW/Massage_pen_FW.ioc) and generated code.
Skeletons: `Massage_pen_FW/User/Src/vibration.c` and separate driver `Massage_pen_FW/User/Src/drv2624.c`,
with matching headers in `Massage_pen_FW/User/Inc`. APIs and control behavior remain undecided.

Motor: LRA with driver frequency tracking. Nominal frequency, maximum sine-wave
RMS voltage and four levels (off plus three voltages) are in `Massage_pen_FW/User/Inc/vibration.h`.
I2C address is in `Massage_pen_FW/User/Inc/drv2624.h`.

IC operation and programming caveats: [DRV2624 guide](drv2624.md).
