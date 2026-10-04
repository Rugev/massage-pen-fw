# Vibration

Communicate with the DRV2624 vibration motor controller over I2C.
The system supports vibration during Normal operation and Hybrid mode.

Use `VIBR_I2C_SCL_Pin` and `VIBR_I2C_SDA_Pin` definitions from
[main.h](../../Core/Inc/main.h). For configuration, consult the read-only
[.ioc](../../Massage_pen_FW.ioc) and generated code.
Skeletons: `User/Src/vibration.c` and separate driver `User/Src/drv2624.c`,
with matching headers in `User/Inc`. APIs and control behavior remain undecided.

Motor: LRA with driver frequency tracking. Nominal frequency, maximum sine-wave
RMS voltage and four levels (off plus three voltages) are in `User/Inc/vibration.h`.
I2C address is in `User/Inc/drv2624.h`.
