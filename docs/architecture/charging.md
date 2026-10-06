# Charging

[charging.c](../../Massage_pen_FW/User/Src/charging.c) owns nonblocking charger
management through the [MP2724 driver](../../Massage_pen_FW/User/Src/mp2724.c).
The [application](../../Massage_pen_FW/User/Src/app.c) owns SYS_ON, operating
states, battery shutdown policy and fault latching. Charging remains requested
during Hybrid operation, subject to charger protections and
available input power; the IC prioritizes system demand. Analog battery voltage
comes from [sensors.c](../../Massage_pen_FW/User/Src/sensors.c).

Pins and ports come from [main.h](../../Massage_pen_FW/Core/Inc/main.h).
Peripheral configuration remains in the read-only
[.ioc](../../Massage_pen_FW/Massage_pen_FW.ioc) and generated code; the
[hardware adapter](../../Massage_pen_FW/User/Src/firmware_hw.c) binds I2C and
callbacks. The [runtime](../../Massage_pen_FW/User/Src/runtime.c) forwards
polled charger interrupt events and uses RAM-retaining GPIO polling for sleep.
IC operation is documented in the [MP2724 reference](mp2724.md).

## Implemented mechanisms

The API, cell limits and agreed setpoints are in
[charging.h](../../Massage_pen_FW/User/Inc/charging.h); register encodings and
transaction contracts are in [mp2724.h](../../Massage_pen_FW/User/Inc/mp2724.h).
The baseline sequence verifies charger inhibit and zero precharge current, then
locks parameters before profile audit, including with a NULL profile or refused
idle lease. Buck operation remains available. Initialization accepts an explicit
reviewed `Charging_Profile`, rather than constructing a profile from factory
defaults. Startup and ordinary wake audit
the configuration, restore differing stable fields with readback verification,
then lock charging parameters. Reserved bits, commands and autonomous fields
are handled separately. Input-limit audits cap the detected limit at the USB
ceiling without increasing it.
Profile validation enforces the agreed cell limits, NTC protection/cool current
reduction, disabled boost and reset/shipping/interrupt requirements.

Configuration of IIN and CHG_CTRL3 requires a board `Charging_IdleOps` lease
through the complete transaction, retries and cancellation drain. A sampled
VIN_RDY does not establish that lease. No production lease or charger profile
is supplied; [main.c](../../Massage_pen_FW/Core/Src/main.c) passes NULL runtime
profiles, so charger configuration readiness and ordinary startup completion
remain gated.

Raw battery eligibility is one admission gate alongside configuration, input and
temperature status. With those gates satisfied, charging is first enabled at zero
precharge current. After the charger is observed in precharge, a newer qualifying
battery sample is required before the sequencer verifies inhibit, unlocks, writes
and reads back the precharge current, relocks and re-enables charging. Phase
departure and normal operation in either normal state drive current to zero at
all requested levels; USB loss, pause and unavailable phase/eligibility also
drive it to zero. A later precharge observation repeats fresh battery
revalidation automatically while fault and retained-completion policy permit it.
Fast-charge, CV and done phases command zero precharge current without toggling
charge enable. Canceled writes remain unknown until drained and reconciled;
transport exhaustion or refused leases keep requested and known hardware state
separately visible.

Status polling and interrupt refresh decode input readiness, charge phase,
top-off, completion and faults. NTC status is treated as fresh only with ready
input and buck enabled, and expires when the status round becomes stale.
Fresh cool status also reduces the application's heater current ceiling.
Warm status pauses charging; clearing warm permits software re-enable only
when charging is required, configuration/input are ready, no protection fault
is present and completion is not retained. Cold/hot and charger faults latch
through the application fault policy.

The charger watchdog is enabled and serviced with valid USB input, including
warm pauses and charge completion. Battery-only operation and sleep preparation
disable it. Watchdog expiry restores/audits configuration only during startup
or ordinary wake validation; runtime expiry faults. Sleep preparation verifies
inhibit and zero precharge current, relocks parameters, disables the watchdog
and drains transport before reporting readiness.

Battery thresholds in [power.h](../../Massage_pen_FW/User/Inc/power.h) select
application sleep or an immediate shipping request. The shipping mechanism verifies the
delay setting under a lease, then sends a single BATTFET disconnect command.
Transport acceptance does not prove physical disconnection; an uncertain
command is reported and never replayed. Without a lease, shipping stays pending.

## Open items

The cell is 1S Li-ion; the NTC1 thermistor is near its centre. The configured
hardware hot fallback exceeds the cell charging limit and does not replace the
MCU warm cutoff; watchdog expiry can restore factory NTC thresholds.
Battery-only NTC freshness and discharge-temperature derating remain unresolved
because VRNTC is documented for buck/boost operation and boost is disabled.
Hardware acceptance must confirm precharge phase visibility at zero current and
measure actual precharge current and transition latency.

Completion survives a warm pause. Automatic recharge rearming after a
warm/completed pause remains an open policy; no rearming action is implemented.
Remaining charger settings, including die thermal regulation, safety timeout
and recharge/top-off choices, require a reviewed profile and hardware
acceptance. Charging display behavior is implemented in
[app.c](../../Massage_pen_FW/User/Src/app.c) and [leds.c](../../Massage_pen_FW/User/Src/leds.c);
production charging remains gated on the reviewed profile and idle lease.
