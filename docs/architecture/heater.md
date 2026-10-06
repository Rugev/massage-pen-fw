# Heater

[heater.c](../../Massage_pen_FW/User/Src/heater.c) implements fixed-point power
control and PWM for Normal and Hybrid operation. The NMOS-switched resistor is
off with control low. Levels, resistance, phase boundaries, temperature
protections and current ceilings are in
[heater.h](../../Massage_pen_FW/User/Inc/heater.h); gains are in
[pid.h](../../Massage_pen_FW/User/Inc/pid.h).

On enable or a target change, temperature below the target band selects
preheat at the permitted power ceiling; temperature above it selects precool
with power off. At the band boundaries, or once preheat/precool reaches them,
control enters PID. Ordinary disturbances do not re-enter those phases.
[pid.c](../../Massage_pen_FW/User/Src/pid.c) applies stronger proportional
feedback above target and conditional integration against the actual
voltage/current-limited power. Battery voltage compensates duty, and the live
average-current ceiling is reconciled every app tick, independently of PID
cadence. Fresh charger cool status or raw tip temperature below the configured
threshold selects the reduced ceiling.

Every fresh valid raw tip reading checks the absolute-temperature inhibit and
the higher latched fault threshold, including equality. Inhibition turns power
off and resumes PID only below the requested target. Faults latch until reboot.
Missing acquisitions retain valid control values during bounded retries;
unavailable sensing, invalid readings, acquisition fault, missing PWM binding
or withdrawn app authorization turns power off.

[sensors.c](../../Massage_pen_FW/User/Src/sensors.c) consumes matched battery/tip
DMA frames, normalizes oversampled ADC counts, checks electrical validity and
filters valid measurements using integer fixed-point state. Tip conversion
uses the battery voltage from the same acquisition to normalize the divider
ratio, followed by integer linear interpolation in
[thermistor_lut.h](../../Massage_pen_FW/User/Inc/thermistor_lut.h).
Thermistor/divider, validity and filter parameters are in
[sensors.h](../../Massage_pen_FW/User/Inc/sensors.h). Regenerate the LUT with
`python3 Massage_pen_FW/scripts/generate_thermistor_lut.py` after changing its
sensor model constants; LUT bounds do not define operating temperature limits.

Use pin/port definitions in [main.h](../../Massage_pen_FW/Core/Inc/main.h).
Consult the read-only [.ioc](../../Massage_pen_FW/Massage_pen_FW.ioc) and
generated code for peripheral configuration. The
[hardware adapter](../../Massage_pen_FW/User/Src/firmware_hw.c) validates ADC/DMA
and heater PWM bindings and starts PWM at zero duty. Mismatched bindings remain
unavailable. The [application](../../Massage_pen_FW/User/Src/app.c) requires
vibration configuration readiness for heat authorization; a nonzero vibration
request additionally requires calibration readiness. Heat-only skips calibration.
The current NULL charger/motor profiles in
[main.c](../../Massage_pen_FW/Core/Src/main.c) leave those readiness gates closed.

PID gains and electrical validity limits are provisional. Measured heater
response, PWM/ADC timing, temperature protection and board acceptance remain
hardware work.
