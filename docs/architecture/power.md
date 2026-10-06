# System power and safety

[power.c](../../Massage_pen_FW/User/Src/power.c) drives `SYS_ON` and monitors
`SYS_PG`; polarities, timeout and battery cutoffs are in
[power.h](../../Massage_pen_FW/User/Inc/power.h). `SYS_ON` powers vibration,
heater and analog sensing. Startup and ordinary wake wait for power good before
sensing/output operation. Power-good acquisition timeout or subsequent loss
becomes a latched app fault; intentional power-off does not.

Sleep currently retains RAM and polls power-button assertion and active-low
charger-interrupt edges. No MCU low-power entry, clock restoration or MCU
watchdog is implemented; [watchdog.c](../../Massage_pen_FW/User/Src/watchdog.c)
remains a skeleton. Low-power mode, watchdog selection and physical acceptance
remain open.

[runtime.c](../../Massage_pen_FW/User/Src/runtime.c) polls raw wake edges during
both active operation and sleep. Edges arriving while shutdown drains are queued
for the sleep handoff. A continuing shutdown hold supplies no new edge and cannot
enable again. A fresh power wake starts ordinary button debounce; a charger wake
retains button identity. Fault wake retains `SYS_ON` off and skips ordinary
validation. Sleep entry requires quiescent ADC/transports, verified charger
inhibit, zero precharge current, parameter relock, watchdog disable and drained
transport, plus inactive outputs, as checked by
[App_GetSnapshot](../../Massage_pen_FW/User/Src/app.c).

Charger bus availability is independent of `SYS_ON`: runtime keeps charger
foreground work enabled while system outputs are powered down. The charger
watchdog is separate from the unimplemented MCU watchdog; its management and
sleep preparation are in [charging.c](../../Massage_pen_FW/User/Src/charging.c).

[App battery policy](application.md) applies standby/disconnect shutdown
thresholds, including equality. The separate raw battery admission threshold can
latch undervoltage before shutdown policy; valid input permits bounded charging
recovery. A charging destination retains `SYS_ON`; a sleep destination disables it
before the notice. Shipping requests remain subject to
the charger idle-register lease. Battery sensing is unavailable with `SYS_ON`
off; standby voltage monitoring remains open.

Pins and ports come from [main.h](../../Massage_pen_FW/Core/Inc/main.h);
peripheral configuration remains in the read-only
[.ioc](../../Massage_pen_FW/Massage_pen_FW.ioc) and generated code.
