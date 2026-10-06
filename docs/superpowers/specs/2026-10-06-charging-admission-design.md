# Charging admission and pre-charge requirements

Reviewed requirements, based on the supplied manufacturer notes. The undervoltage
fault applies in all powered operation, as confirmed by the user.
This supersedes the provisional pre-charge requirements in
[charging.md](../../architecture/charging.md). Other requirements and module
ownership in the [runtime design](2026-10-04-runtime-architecture-design.md)
remain in effect.

## Charging admission

- During startup and configuration validation, inhibit charging with `EN_CHG`
  and establish a verified zero pre-charge current before admission. Keep heat
  and vibration off during initial validation.
- Require a valid, settled ADC battery reading and the existing configuration,
  input, temperature and fault checks before permitting charging.
- Use the valid, unfiltered ADC voltage directly, without an additional voltage
  margin. Invalid or unavailable readings must not authorize charging; retained
  past readings alone do not establish current eligibility.
- A valid reading below 2500mV latches an undervoltage fault, including during
  battery-only operation. Charging is prohibited; voltage recovery, USB
  reconnection and RAM sleep/wake do not clear the fault. Reboot follows the
  existing fault lifecycle and must validate the battery again.
- Exactly 2500mV passes the voltage admission check. Other checks still apply.
- Keep this admission threshold separate from the cell discharge endpoint and
  existing application standby/disconnect policy, whose definitions remain in
  [charging.h](../../../Massage_pen_FW/User/Inc/charging.h) and
  [power.h](../../../Massage_pen_FW/User/Inc/power.h).

## Pre-charge and phase behavior

- Change the IC's nominal pre-charge/fast-charge transition to 3000mV.
- Separate safe pre-charge current (0mA) from permitted operating pre-charge
  current (240mA). Permit operating pre-charge only after admission.
- Zero is the required pre-charge setting outside admitted pre-charge in the
  nominal 2500–3000mV region. Restore and verify zero on USB removal, normal
  operation, charging inhibition or pause, and any non-pre-charge IC phase.
  Invalid or unavailable phase/eligibility information cannot retain permission
  for operating pre-charge current.
- Normal operation includes charging while heat/vibration operation is enabled;
  pre-charge current remains zero in both normal-operation application states.
- Let the IC select the transition using its native tolerance and hysteresis;
  do not replace that transition with an MCU voltage margin.
- Use `CHG_STAT` to detect departure from pre-charge, including fast charge
  (constant current), constant voltage and done. Ensure zero even when the
  first observed phase is already outside pre-charge.
- If the IC subsequently returns to pre-charge, retain zero until the MCU
  revalidates voltage and deliberately permits operating pre-charge again.
  Firmware may perform this revalidation and rearming automatically when all
  checks pass; no user action is required. A latched fault still prohibits it.
- Preserve the existing fast-charge, regulation, termination and zero-trickle
  targets in [charging.h](../../../Massage_pen_FW/User/Inc/charging.h).
  Pre-charge updates must preserve termination and unrelated register fields.
- Configuration auditing must recognize intentional pre-charge changes rather
  than overwrite them with a fixed operating setting.

## Locking, failures and lifecycle

- Increasing pre-charge current requires explicit unlock, write, readback
  verification and relock. Coordinate this sequence with charging enable so
  charging cannot start before admission and successful parameter verification.
  The later reduction to zero is permitted while locked.
- Failed writes, readbacks and mismatches follow the existing communication
  failure policy. Unverified changes cannot authorize charging. Pending work
  must not enable charging after eligibility is revoked.
- USB reconnection, MCU restart and startup/wake watchdog recovery require
  renewed validation before charging permission. Runtime watchdog expiry
  remains a latched fault under the runtime design.
- Preserve existing temperature limits, warm-pause behavior, watchdog servicing
  during warm pauses and completion, USB input ceiling and completion handling.
  A temperature resume must still satisfy voltage eligibility.
- Fault handling must request charging inhibition. A failed or unavailable
  transport cannot be reported as verified physical inhibition.
- Phase-triggered writes use the existing nonblocking transport promptly.
  Response time includes status-refresh and I2C transaction/retry latency;
  instantaneous switching is not required or guaranteed.

Zero pre-charge current inhibits only the pre-charge phase. It does not inhibit
fast charging, and charger watchdog expiry/reset can restore nonzero factory
currents and charging enable. Retain `EN_CHG` inhibition during validation;
zero current is an additional precaution, not a guarantee against autonomous
charging before firmware gains control or after communication failure.

## Charging and battery LED behavior

The battery LED has independent priority from the heat/vibration LEDs.

- An error overrides battery green indications with the existing red error
  blink. Preserve the six-bit fault display while the fault display is active.
- Active charging uses green breathing, including charging while normal
  operation. Normal operation without active charging uses solid green.
- Charger input presence or a charge request alone does not mean active charging.
  Use fresh admitted charging status, including active top-off, for breathing.
- During normal operation, heat/vibration level and heater-phase displays take
  precedence over charging progress. Outside normal operation, active charging
  uses the vibration LEDs for voltage progress; heat LEDs remain off.
- Use these coarse display bands, selected under the user's authorization:

| Filtered valid battery voltage | Vibration indication during charging only |
| --- | --- |
| Below 3600mV | First blinking; second/third off |
| At least 3600mV and below 3900mV | First solid; second blinking; third off |
| At least 3900mV | First/second solid; third blinking |

These bands are voltage indications, not measured state-of-charge percentages.
Use filtered voltage only for display; admission/protection still use raw data.
Do not show progress from unavailable/invalid battery data. Store boundaries
and animation settings in [leds.h](../../../Massage_pen_FW/User/Inc/leds.h).
Use a one-second blink cycle and the existing breathing/error periods. Maintain
battery and level-pattern phases independently so changing a voltage band or
requested level does not restart battery breathing or red error blinking.

Completion and non-error pause indications, and whether red error blinking
continues during fault sleep, await the user's answer. Keep the existing
low-battery notice unless an error overrides it; preserve ordinary sleep-off
behavior. Do not implement an assumed change to fault-sleep behavior.

## Ownership and references

- [app.c](../../../Massage_pen_FW/User/Src/app.c) owns application policy,
  fault latching, heat/vibration inhibition and LED indication priority.
- [leds.c](../../../Massage_pen_FW/User/Src/leds.c) renders requested battery
  and heat/vibration patterns without blocking the application.
- [sensors.c](../../../Massage_pen_FW/User/Src/sensors.c) provides ADC battery
  measurements and validity.
- [charging.c](../../../Massage_pen_FW/User/Src/charging.c) owns charger
  admission sequencing, phase handling, configuration audit and watchdog service.
- [mp2724.c](../../../Massage_pen_FW/User/Src/mp2724.c) and
  [mp2724.h](../../../Massage_pen_FW/User/Inc/mp2724.h) provide nonblocking
  register operations and field definitions. IC behavior is referenced in
  [mp2724.md](../../architecture/mp2724.md).
- New hardware/tunable values belong in the relevant user headers. Follow
  [architecture.md](../../../architecture.md) and
  [AGENTS.md](../../../AGENTS.md); protected/generated files remain unchanged.

## Acceptance criteria

- Exercise valid readings below, exactly at and above admission, invalid/missing
  ADC data, settling and unfiltered undervoltage detection during operation.
- Verify undervoltage retention across voltage recovery, USB cycling and RAM
  sleep/wake, with and without input power.
- Verify initial inhibition, operating pre-charge, first-observed fast/CV/done,
  reduction to zero, return to pre-charge and deliberate revalidation.
- Verify zero pre-charge on normal operation, USB removal, temperature pauses,
  other non-pre-charge phases and loss of valid eligibility/phase information.
- Verify lock ordering, preservation of unrelated fields and audit coordination.
- Inject communication failures and eligibility loss during parameter changes;
  verify they cannot authorize charging or claim unverified inhibition.
- Exercise restart, watchdog expiry/recovery, USB reconnection, temperature
  pauses and completion through host tests. Build Debug with
  `./Massage_pen_FW/build.sh` when implementation is undertaken.
- Verify voltage-band boundaries, blinking versus solid outputs, simultaneous
  heater/battery breathing, error priority, normal-operation level precedence,
  top-off status and retained animation phase across unrelated display changes.
- On hardware, verify currents, phase/status behavior, lock behavior and response
  latency, including reset/watchdog conditions. Host tests do not establish
  physical charging inhibition.

## Open decisions

Public interface signatures, the undervoltage fault display code/priority and
the precise pre-charge revalidation sequence remain open for planning. Automatic
pre-charge revalidation does not define a new post-completion recharge policy.
The complete production charger profile,
hardware idle lease, battery-only NTC freshness and other existing charging open
items remain unresolved; this spec does not supply them.
LED completion/pause indications and fault-sleep error blinking remain open.
