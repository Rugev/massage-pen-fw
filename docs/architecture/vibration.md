# Vibration

[vibration.c](../../Massage_pen_FW/User/Src/vibration.c) implements nonblocking
LRA configuration, calibration and constant-amplitude closed-loop RTP control
for Normal and Hybrid operation, through the
[DRV2624 driver](../../Massage_pen_FW/User/Src/drv2624.c). Motor requirements,
levels and the explicit `Vibration_Profile` API are in
[vibration.h](../../Massage_pen_FW/User/Inc/vibration.h); register encodings and
transaction contracts are in [drv2624.h](../../Massage_pen_FW/User/Inc/drv2624.h).
IC operation and programming caveats remain in the [DRV2624 guide](drv2624.md).

On normal-operation entry, the module checks driver status, stops activity,
observes idle and loads/verifies the supplied configuration. The first nonzero
request without retained calibration runs fixed-duration auto-calibration with
bounded attempts and fresh completion/result evidence. Success captures
compensation and BEMF gain in RAM; power loss invalidates readiness, and the
next entry restores/verifies those results. RAM sleep retains calibration;
reboot clears it. Flash persistence remains deferred.

Playback applies the profile's validated level-to-RTP mapping and observes GO
before reporting active output. Off/disable cancels and drains transfers, stops
the process and waits for braking/standby settlement. Configuration of
autonomously updated calibration registers occurs only while activity is idle,
with ownership retained through retries and cancellation drain. Status polling
shares retained read-to-clear evidence with the application; driver,
communication and exhausted-calibration failures latch through application
fault policy. Heating with a nonzero vibration request waits for calibration
success or verified restoration.

Pins and ports come from [main.h](../../Massage_pen_FW/Core/Inc/main.h).
Consult the read-only [.ioc](../../Massage_pen_FW/Massage_pen_FW.ioc) and
generated code for configuration. The
[hardware adapter](../../Massage_pen_FW/User/Src/firmware_hw.c) binds I2C and
callbacks; [runtime.c](../../Massage_pen_FW/User/Src/runtime.c) accepts reviewed
profiles. No production motor profile is supplied and
[main.c](../../Massage_pen_FW/Core/Src/main.c) passes NULL profiles, so vibration
configuration and calibration readiness remain false.

The complete motor/input profile, timing and RTP strength mapping still require
bench validation. The implemented state machine does not establish motor
voltage accuracy, resonance tracking or physical acceptance.
