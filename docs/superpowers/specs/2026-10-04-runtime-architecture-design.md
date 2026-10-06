# Runtime architecture: agreed requirements

Runtime startup and peripheral adapters connect through [runtime.c](../../../Massage_pen_FW/User/Src/runtime.c) and [firmware_hw.c](../../../Massage_pen_FW/User/Src/firmware_hw.c). Runtime implements polling sleep while retaining RAM; true MCU low-power entry remains deferred. This records the agreed design; unresolved items below remain open. Values belong in user headers, not this document.

## Ownership and execution

- [app.c](../../../Massage_pen_FW/User/Src/app.c) owns operating policy,
  transitions, fault latching, session timers, requested levels and module coordination.
  The current application state is declared in [app.h](../../../Massage_pen_FW/User/Inc/app.h).
- [main.c](../../../Massage_pen_FW/Core/Src/main.c) remains a minimal entry point,
  initializes generated peripherals and passes HAL handles to [runtime](../../../Massage_pen_FW/User/Src/runtime.c).
- [runtime.c](../../../Massage_pen_FW/User/Src/runtime.c) dispatches app work once
  per distinct HAL tick; it does not catch up missed ticks and records skipped
  slots. Sensor cadence follows [sensors.c](../../../Massage_pen_FW/User/Src/sensors.c).
- [firmware_hw.c](../../../Massage_pen_FW/User/Src/firmware_hw.c) adapts board
  peripherals. [i2c_device.c](../../../Massage_pen_FW/User/Src/i2c_device.c)
  provides the common asynchronous transfer engine used by each dedicated I2C
  driver/instance. Transfers are stopped and drained before buffers are reused.
- Startup currently passes NULL optional profiles; readiness-dependent policy
  gates remain active until required validated configurations are supplied.
- Run foreground app work at 1 kHz using SysTick timing. ADC and I2C transfers
  use interrupts; callbacks capture results/events for foreground processing.
- Modules own mechanisms and internal sequencing. App gets status and supplies
  commands; module sequencing must not block the app loop.
- Use integer/fixed-point math as required by [architecture.md](../../../architecture.md).

| Module | Responsibility |
| --- | --- |
| `runtime` | Foreground dispatch and polling sleep |
| `firmware_hw` | Board peripheral adapters and callback routing |
| `i2c_device` | Common asynchronous transfer engine for dedicated I2C instances |
| `power` | SYS_ON GPIO sequencing, SYS_PG monitoring and wake polling |
| `sensors` | ADC acquisition, battery/tip conversion and validity |
| `charging` / `mp2724` | Charger management / device transport and register operations |
| `vibration` / `drv2624` | Level control and calibration / device operations |
| `heater` / `pid` | Heater phases, power limits and protections / control calculation |
| `buttons` | Debouncing and press/release events; app interprets gestures |
| `leds` | Requested display patterns |
| `storage` | Settings load; dummy heat/vibration level 1 for now |
| `watchdog` | Stub; MCU watchdog selection remains open |

Sources and matching headers are in [User/Src](../../../Massage_pen_FW/User/Src)
and [User/Inc](../../../Massage_pen_FW/User/Inc).

## Application states

| State | Required behaviour |
| --- | --- |
| `APP_STARTUP` | Cold-boot validation, settings load, restart inference |
| `APP_WAKEUP` | Ordinary sleep-exit validation and settings load |
| `APP_SLEEP` | SYS_ON disabled; retain RAM |
| `APP_CHARGING` | Charging management; normal operation disabled |
| `APP_NORMAL` | Normal operation without input power |
| `APP_CHARGING_NORMAL` | Charging while normal operation; normal-operation LEDs take precedence |
| `APP_CHARGING_RECOVERY` | Low battery with valid input but inactive charging; SYS_ON enabled, outputs inhibited, bounded charging-start wait |
| `APP_LOW_BATTERY_NOTICE` | Outputs disabled; red flashes; destination Charging or Sleep |
| `APP_BATTERY_DISCONNECT` | Immediate charger shipping request |
| `APP_FAULT_DISPLAY` | SYS_ON disabled; latched fault display |
| `APP_FAULT_SLEEP` | SYS_ON disabled; retain fault in RAM |

Startup/wake enable SYS_ON and wait for SYS_PG before battery/tip sensing or
output operation. Failure to obtain power good within 1 s, or loss of power good
after acquisition while SYS_ON remains enabled, faults. Intentional power-off
does not constitute a power-good fault.

Validation acquires measurements, applies battery policy, checks charger errors
and verifies/restores charger configuration. Charger watchdog expiry is recoverable
only on startup or ordinary sleep exit; runtime expiry faults.

Load storage settings on cold boot and ordinary RAM sleep exit. Only cold boot
checks inferred heater overtemperature: tip at or above the heater fault threshold
minus 2 degrees clears both requested levels. Low battery takes precedence over
that inference. Loading levels alone never enables outputs.

After validation, an accepted enable request enters Normal or Charging while
normal operation when interlocks permit. Otherwise enter Charging with input
present, or Sleep without input. A waking press may supply the enable request.
A press released before the enable duration still allows validation to finish.

USB insertion/removal switches between Normal and Charging while normal
operation without restarting the session. User shutdown or timeout disables
normal operation and selects Charging with input present, otherwise Sleep.

## Battery policy

Thresholds are in [power.h](../../../Massage_pen_FW/User/Inc/power.h).
Heating and vibration require voltage above the standby threshold and a qualifying
enable press. At or below that threshold ends normal operation; voltage recovery
alone does not resume it.

Without input, apply the standby/disconnect shutdown policy. With input and low
battery, allow 10 s for the charger to report an active charging phase. A reported
active phase cancels the deadline. If charging later stops while battery remains
low, start a new 10 s deadline; expiry applies the battery shutdown policy.
Keep SYS_ON enabled during charging recovery, with heat/vibration inhibited.

When low voltage ends charging while normal operation, display the low-battery
notice before entering Charging; charging management continues. Keep SYS_ON
enabled for a Charging destination. For a Sleep destination, disable SYS_ON
immediately, display the notice, then sleep. The lower disconnect threshold
requests immediate shipping when shutdown policy applies.
Equality applies to the disconnect threshold too: at or below it, request shipping
when shutdown policy applies. Exactly the standby threshold does not permit enable.

## Acquisition and timing

- Acquire one fresh battery/tip pair every 1 ms. Each app loop consumes the
  previous completed pair and triggers the next acquisition, which must complete
  before the following loop. Use a circular ADC buffer with half/full callbacks.
  Publish completed frames for foreground consumption without reading a half
  currently being written. ADC operation must not free-run faster than this cadence.
- A missing/failed frame counts once per scheduled acquisition; fault after ten
  consecutive failures, separately per channel. A valid acquisition resets its
  channel count. The next-loop completion requirement replaces the earlier
  proposed 2 ms conversion timeout; retries use the next 1 ms acquisition slot.
- Apply integer low-pass filtering to valid measurements for control. Use
  unfiltered measurements for electrical validity and heater temperature protection.
  Initialize filters from the first valid pair after sensing is restored.
  Use coefficient 1/16: filtered += (sample - filtered) / 16, retaining fractional
  precision in fixed-point state.
- Allow 10 ms settling after SYS_PG becomes good before initial acquisition.
- I2C timeout: 5 ms per transfer. Retry after 5 ms and completion/abort of the
  previous transfer; a configuration attempt includes write and readback.
- Poll charger/driver status every 100 ms; charger interrupts also prompt reads.
- Heater control updates every 50 ms. Evaluate heater temperature protections
  on every completed valid tip measurement, independently of PID timing.
- Button debounce: 20 ms stable input for press and release.

Initial electrical validity limits are provisional for bench validation:
tip ADC input within 20 mV of either rail faults; battery above 4500 mV faults.
Do not assign a lower battery sensor-fault cutoff; valid low voltage follows
shutdown policy. Normalize the configured oversampled ADC results before
conversion. Values are defined in [sensors.h](../../../Massage_pen_FW/User/Inc/sensors.h).

The hardware adapter validates the generated two-channel ADC sequence and configures circular DMA and callback routing in [firmware_hw.c](../../../Massage_pen_FW/User/Src/firmware_hw.c). The protected generated configuration remains subject to regeneration review; generated files are not hand-edited.

## User interface

- Power hold of 0.3 s requests enable; the waking press counts. Shorter presses
  do not enable normal operation. One press cannot both enable and disable.
- A fresh 1 s power hold during normal operation disables it.
- Heat/vibration clicks last more than 0.1 s and release before 0.5 s;
  cycle requested levels while normal operation is enabled.
- End the entire normal-operation session after 75 min, regardless of level
  changes or USB transitions. Also end after both requested levels remain zero
  for more than 5 min.
- Low-battery notice: three red flashes, each 500 ms on and 500 ms off.
- Fault display: red battery LED 100 ms on/100 ms off; six heat/vibration LEDs
  display one solid 6-bit error code.
- Heater preheat/precool: active level LEDs breathe with a 2 s period;
  PID operation uses solid level LEDs.

Pins and existing level definitions remain in [UI references](../../architecture/ui.md)
and user headers. Charging-only display details remain a separate charging topic.

## Heater and vibration

Heater uses battery-compensated power control, fixed-point PID with integral
anti-windup, and extra negative feedback above target. Current ceilings refer
to average current: 1 A for charger cool status or tip below 10 degrees,
otherwise 2 A. Update the ceiling when these conditions change.
At exactly 10 degrees, the tip condition alone does not reduce the ceiling.

Select preheat/precool only on heating enable or target change. Below target
minus 2 degrees, preheat at maximum permitted power until that boundary.
Above target plus 2 degrees, precool with power off until that boundary.
Otherwise enter PID; ordinary disturbances do not re-enter preheat/precool.
Exactly either transition boundary selects PID on enable/target change.

Split the existing absolute-temperature protection into a 49-degree power
inhibit and a 49.5-degree latched fault. After inhibition, resume PID only below
target, with integral handling to avoid windup. Hardware overheat protection
remains independent. Put these values in [heater.h](../../../Massage_pen_FW/User/Inc/heater.h).
Both protection thresholds trigger at equality as well as above.

Provisional bench-tuning gains: proportional 500 mW/degree, integral
20 mW/(degree second), derivative zero initially. Double proportional feedback
for negative temperature error. Use conditional integration against the actual
voltage/current-limited output, allowing integration that exits saturation.
Final gains require measured tip response; define them in
[pid.h](../../../Massage_pen_FW/User/Inc/pid.h).

Vibration uses constant-amplitude closed-loop operation. On normal-operation
entry check driver errors and load/verify configuration. On the first nonzero
vibration request without calibration, calibrate with up to three attempts total;
failure faults. Keep successful results in RAM, restore/verify after driver power
loss, and reuse across RAM sleep. Reboot requires calibration again. Flash
persistence is deferred. When heat and vibration are requested together, heat
waits for calibration success or verified restoration. Heat-only skips
calibration but still requires validated vibration configuration.
See [DRV2624 requirements](../../architecture/drv2624.md).

## Failures and fault lifecycle

- An I2C configuration attempt succeeds only after write and matching readback.
  Failed write, failed readback or mismatch counts as a failure; three consecutive
  failures fault. Compare configuration fields according to register semantics.
- Standalone I2C reads also fault after three consecutive failures. Success resets
  the corresponding failure count.
- ADC acquisition faults after ten consecutive bad acquisitions; success resets
  the count. Heating is not disabled during acquisition retries and uses the last
  valid measurements. Startup still requires valid initial measurements.
- An acquired tip reading indicating an open/short thermistor, or invalid battery
  voltage, faults immediately. Valid low voltage follows battery policy.
- Faults include battery cold/hot, power-good failure, charger/driver errors,
  heater fault temperature, communication failure and sensor failure.
- All faults latch until reboot, including battery cold/hot. Disable SYS_ON and
  display the fault for 60 s, then enter RAM-retaining fault sleep.
- Fault wake keeps SYS_ON off, skips measurements/ordinary wake validation,
  displays the retained fault for 10 s and sleeps again; LEDs are off in
  `APP_FAULT_SLEEP`.
- Ignore the usual 1 s shutdown gesture in fault states. A 10 s button hold is
  intended to trigger the charger hardware power reset; firmware does not
  guarantee that reset and the wiring requires hardware verification.

Latch the first detected fault. For faults detected in the same app cycle,
use the code order below as priority. Subsequent faults do not replace the code.
Bit order: heat LEDs 1/2/3 are bits 0/1/2; vibration LEDs 1/2/3 are bits 3/4/5.

| Code | Fault |
| --- | --- |
| 1 | Heater fault temperature |
| 2 | Invalid tip sensor |
| 3 | Invalid battery voltage |
| 14 | Battery below charging admission threshold |
| 4 | SYS_PG timeout |
| 5 | SYS_PG lost |
| 6 | Battery hot |
| 7 | Battery cold |
| 8 | Charger-reported error |
| 9 | Vibration-controller error |
| 10 | Charger communication/readback failure |
| 11 | Vibration communication/readback failure |
| 12 | ADC acquisition failure |
| 13 | Vibration calibration failure |

## Open items for planning

- Battery-only STATUS3 freshness remains unresolved; do not assume live NTC data.
- Remaining charger profile/settings and recharge policy remain unresolved;
  preserve [existing charging requirements](../../architecture/charging.md).
- Validate provisional electrical validity limits and tune heater gains on hardware.
- Agree the validated motor configuration/calibration timing, production idle lease and MCU watchdog. No default charger or motor profile or idle lease is supplied; charging and vibration readiness remain gated until approved profiles and lease are provided.
- Select a RAM-retaining low-power mode and verify wake/reset wiring and generated
  configuration. Report any required protected-file changes; do not hand-edit them.
- User-setting saving and persistent calibration remain deferred. The
  persistence format and save interface remain undefined; settings loading is
  provided by [storage.c](../../../Massage_pen_FW/User/Src/storage.c).

Implementation verification should exercise state transitions and failure counts
with simulated peripherals, build Debug with the prescribed build script, and
check interrupt timing, power sequencing, sleep/wake, calibration and heater
response on hardware. This design review does not establish hardware verification.
