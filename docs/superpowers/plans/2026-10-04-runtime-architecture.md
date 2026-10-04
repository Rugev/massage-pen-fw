# Runtime Architecture Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans for native execution, or superpowers:subagent-driven-development if the user selects delegation. Execute task-by-task using the checkboxes below.

**Goal:** Implement modular firmware runtime control, user interaction and latched safety handling according to the reviewed design.

**Architecture:** App owns the single application state and policy; modules own their mechanisms and sequences. A SysTick-timed foreground loop consumes interrupt-driven acquisition/transfer results and advances the system without blocking. Hardware-dependent completion is gated on generated configuration and validated settings.

**Tech Stack:** STM32G030, STM32 HAL, C, integer/fixed-point arithmetic, CMake/Ninja Debug build; host C tests with fake peripherals.

**Spec:** [Runtime design](../specs/2026-10-04-runtime-architecture-design.md).

## Global constraints

- Read [AGENTS.md](../../../AGENTS.md), [architecture.md](../../../architecture.md), the spec, then relevant topic references.
- User code belongs in `Massage_pen_FW/User/Inc` and `Massage_pen_FW/User/Src`.
- Do not edit Drivers, Core except existing `main.c` USER CODE blocks, the IOC, generated CMake, startup assembly or linker script. Preserve main.c markers and line endings.
- Use generated pin/port definitions and explicit HAL handles. Add any new production sources only through `Massage_pen_FW/User/CMakeLists.txt`.
- Put agreed settings in relevant user-header macros; do not copy register constants/configuration into documentation.
- Use integer/fixed-point arithmetic with wide intermediates; preserve fractional filter/PID state.
- Run firmware builds only with `./Massage_pen_FW/build.sh`; keep outputs untracked.
- Public APIs remain open as required by repository guidance. Interface sections below specify responsibilities/data, not unagreed signatures. Select concrete APIs during each task, retaining app-owned policy.
- Preserve user changes; do not start hardware outputs using guessed charger/motor settings.

## Scope and prerequisites

Tasks 1–8 deliver independently tested mechanisms and coordinated policy. Task 9 connects and validates them on hardware. Detailed charging configuration still needs a separate charging design/plan using [charging.md](../../architecture/charging.md); Task 7 implements the agreed integration behaviour without choosing remaining settings.

Before hardware integration, the user must provide/regenerate:

- Two-channel ADC sequencing, circular DMA and half/full DMA interrupt routing. Acquisition must support a new pair every app tick, without free-running or restarting an already active conversion.
- Wake routing compatible with a RAM-retaining low-power mode for both power button and charger interrupt. Current wake-pin selection alone does not establish this behaviour.
- Verified hardware button/reset wiring and polarities, including the 10 s charger power reset. Do not implement an assumed software reset as its replacement.
- An agreed motor configuration and calibration duration. Successful calibration requires the assembled actuator.
- A resolution of battery-only STATUS3 freshness, or an explicitly agreed operating policy for unavailable NTC status. Treat this as a hardware release prerequisite, not evidence of a fresh measurement.
- MCU watchdog selection/configuration and behaviour through sleep. Do not enable one until its integration is agreed.

## Review focus

- A completion interrupt after cancellation/power-off must not publish an old frame or complete a new operation: Tasks 2 and 3.
- Timer rollover and a waking press spanning validation must preserve gesture/deadline semantics: Tasks 1 and 8.
- USB disappearing during low-battery notice/recovery must immediately restore battery-only shutdown policy: Task 8.
- A failed operation must not have its failure count reset by unrelated successful work: Tasks 2 and 3.
- Fault sleep must retain its latch and skip sensing even after button/charger wake: Tasks 8 and 9.

## Verification approach

Create `tests/runtime/run.sh` to compile selected production module code with a host C compiler and fake HAL/peripheral adapters, using temporary build outputs. Accept task selectors listed below; all selectors together run with no argument. Tests should assert observable commands, results and transitions, not copies of implementation expressions.

For each task: add the listed failing cases first, run the selector to confirm failures, implement the deliverable, rerun to pass, then commit only task files. Callback injection and simulated time keep long-duration tests deterministic. Host testing does not establish DMA timing, thermistor validity or physical heater stability.

### Task 1: Power sequencing and time handling

**Files:** Modify `Massage_pen_FW/User/{Inc/power.h,Src/power.c}`. Create `tests/runtime/{run.sh,fake_hal.h,fake_hal.c,test_power.c}` and only the stub headers needed by the host harness.

**Interfaces:** Power accepts enable/disable and exposes starting/ready/failure; app owns the consequences. GPIO/time adapters allow host simulation. Low-power entry is connected in Task 9 after mode selection.

- [ ] Write `test_power.c`: disabled by default; wait for PG; PG at 999 ms succeeds; no PG at 1000 ms fails; loss after good fails; intentional disable does not fault; elapsed-time checks work across tick rollover.
- [ ] Run `./tests/runtime/run.sh power`; confirm the missing behaviour fails.
- [ ] Implement power sequencing with nonblocking timing and generated pins/polarities. Keep physical low-power entry separated from policy; do not use HAL_Delay.
- [ ] Run the selector and Debug build; commit the task files after both pass.

### Task 2: ADC frames, conversion, filtering and failure tracking

**Files:** Modify `Massage_pen_FW/User/{Inc/sensors.h,Src/sensors.c}`. Create `tests/runtime/test_sensors.c`; extend fake HAL. Use existing `Inc/thermistor_lut.h` and generation script without duplicating the table.

**Interfaces:** Sensors receives ADC handle and power availability; publishes matched battery/tip values, unfiltered validity/protection data, filtered control data and acquisition failures. App schedules acquisition; DMA callbacks publish completed frames.

- [ ] Write cases for half/full frame alternation; one pair per tick; no mixed epochs or duplicate consumption; next-tick missing completion counts once; ninth failure survives and tenth faults; success resets only that channel. Inject late callbacks after stop/restart and assert no old data is accepted.
- [ ] Add conversion/filter cases: oversampling normalization, matched-battery thermistor normalization, integer interpolation, first-value filter initialization, coefficient 1/16 convergence in both directions, provisional rail/high-voltage invalidity, and valid undervoltage distinguished from sensor fault. Assert unfiltered protection data bypasses filter lag.
- [ ] Run `./tests/runtime/run.sh sensors` and confirm the cases fail; implement acquisition/frame ownership and conversion/filtering. Start only after power good plus agreed settling time. Suspend acquisition and invalidate readiness when sensing power is removed.
- [ ] Run the selector and Debug build; commit. Until DMA generation is available, test the frame mechanism through callbacks/fakes and keep hardware acquisition explicitly gated rather than silently substituting another cadence.

### Task 3: Interrupt-driven device operations and verified writes

**Files:** Modify `Massage_pen_FW/User/{Inc/mp2724.h,Src/mp2724.c,Inc/drv2624.h,Src/drv2624.c}`. Create `tests/runtime/test_i2c.c`; extend fake HAL. Preserve existing register metadata.

**Interfaces:** Each driver receives its I2C handle, progresses one pending operation, retains completion/status and reports exhausted retries. Higher modules request status reads or configuration writes with field-aware verification.

- [ ] Write cases for 5 ms transfer timeout and retry delay; failed write/readback/mismatch counts as one configuration failure; only matching readback succeeds; three consecutive attempts fault; standalone reads follow the same limit; unrelated successes do not reset a pending operation's counter.
- [ ] Add tests for reserved-bit preservation, suppression of unrelated action bits, command-aware verification, automatically changing fields, stale callbacks after abort/restart, and a single retained DRV STATUS snapshot despite read-to-clear semantics. Do not blindly reissue commands whose side effects differ from configuration writes.
- [ ] Run `./tests/runtime/run.sh i2c` to confirm failure; implement nonblocking transport, timeout/abort ownership and register-aware transactions. Do not read DRV STATUS independently from multiple consumers.
- [ ] Run the selector and Debug build; commit. If a shared transport helper becomes necessary, explicitly register its source in User CMake; keep charger/vibration policy out of it.

### Task 4: Buttons, displays and dummy settings

**Files:** Modify `Massage_pen_FW/User/{Inc/buttons.h,Src/buttons.c,Inc/leds.h,Src/leds.c,Inc/storage.h,Src/storage.c}`. Create `tests/runtime/test_ui.c`.

**Interfaces:** Buttons publish debounced press/release information; app interprets duration by state. LEDs accept a display request and advance patterns. Storage loads dummy levels without writing flash.

- [ ] Write cases for 20 ms stable transitions, bounce, durations spanning rollover, level-count LEDs, 2 s breathing restricted to active heater LEDs, three 1 s red flashes, 0.2 s fault flashing and the agreed six-bit order. Dummy loads must yield 1/1 on each cold/ordinary wake load request.
- [ ] Run `./tests/runtime/run.sh ui`; confirm failure. Implement mechanisms and header macros without embedding app transitions in buttons/LEDs. Keep persistent saving and calibration storage deferred.
- [ ] Run the selector and Debug build; commit. Software LED modulation must preserve the app loop's execution budget; inspect smoothness later on hardware.

### Task 5: Heater power control and protections

**Files:** Modify `Massage_pen_FW/User/{Inc/heater.h,Src/heater.c,Inc/pid.h,Src/pid.c}`. Create `tests/runtime/test_heater.c`.

**Interfaces:** Heater accepts requested level, valid measurement snapshot, enable/readiness and average-current ceiling; owns PWM/phase and reports faults. PID accepts error/cadence and actual output bounds. App owns battery-temperature policy and LEDs.

- [ ] Write cases for voltage-compensated power/duty, 1 A/2 A average-current limits, saturation and integral recovery, negative-error extra feedback, and provisional gains/cadence from the spec. Verify wide intermediates across the accepted sensor range.
- [ ] Add phase cases: enable/target change chooses preheat/precool only outside the band; boundary equality selects PID; phase completion goes to PID; later disturbances stay in PID; level zero disables. At 49 degrees inhibit until strictly below target; at 49.5 degrees report fault immediately on fresh unfiltered data. ADC acquisition retries retain last valid control data.
- [ ] Run `./tests/runtime/run.sh heater`; confirm failure. Implement fixed-point control with conditional integration and power/current clamps, keeping PWM zero until app authorizes operation. Reset/adjust integral on phase/inhibit transitions to avoid windup; final thermal behaviour is verified in Task 9.
- [ ] Run the selector and Debug build; commit. Keep provisional tuning identifiable in headers; do not claim stable hardware control from host tests.

### Task 6: Vibration readiness and RAM calibration

**Files:** Modify `Massage_pen_FW/User/{Inc/vibration.h,Src/vibration.c}`. Create `tests/runtime/test_vibration.c`.

**Interfaces:** Vibration consumes DRV operations, requested level and power availability; reports configuration/calibration readiness, retained results and failures. App gates heating when both outputs are requested.

- [ ] Write cases for error check before configuration, verified configuration, no calibration for level zero, first nonzero request calibrating, three failed calibrations faulting, and success retaining all three calibration results in RAM.
- [ ] Add cases for power loss restoring/verifying retained results, RAM sleep preserving them, reboot requiring calibration again, constant RTP level changes, and cancellation during calibration never publishing a false success. Clear-to-read status must remain observable to app.
- [ ] Run `./tests/runtime/run.sh vibration`; confirm failure. Implement the nonblocking sequence with Task 3 operations and validated-profile input. Do not apply default/example motor settings as a board profile. Physical calibration waits for the motor configuration prerequisite.
- [ ] Run the selector and Debug build; commit. Flash writes remain absent.

### Task 7: Charger integration contract

**Files:** Modify `Massage_pen_FW/User/{Inc/charging.h,Src/charging.c}`. Create `tests/runtime/test_charging.c`. Read `docs/architecture/{charging,mp2724}.md`.

**Interfaces:** Charging consumes MP2724 operations and an agreed board configuration; reports input validity, active charge phase, NTC status/freshness, errors and verified readiness. App owns low-voltage deadlines and shipping decisions; charging executes requests and maintains USB-dependent watchdog service.

- [ ] Write cases distinguishing input-valid, input-ready and active charge phase; configuration write/readback verification; startup/wake watchdog recovery versus runtime expiry; 100 ms polling plus interrupt-triggered refresh; shipping request and unrelated-command suppression.
- [ ] Add cases that watchdog service continues with valid USB during temperature pauses/charge completion, is disabled on battery-only/pre-sleep paths, and charger status acquisition cannot hide errors. Preserve existing warm-pause/charge-completion requirements; unavailable battery-only NTC status must not be labelled fresh.
- [ ] Run `./tests/runtime/run.sh charging`; confirm failure. Implement the agreed monitor/command contract with Task 3, retaining existing charging limits. Leave configuration readiness false if required board settings are unresolved; host tests supply an explicit fake agreed profile.
- [ ] Run the selector and Debug build; commit. Complete the separate charging design/configuration work before enabling charging control on hardware; this task does not invent the remaining setpoints or charging display policy.

### Task 8: App state machine and policy

**Files:** Modify `Massage_pen_FW/User/{Inc/app.h,Src/app.c}`. Create `tests/runtime/test_app.c` and module fakes as needed.

**Interfaces:** App consumes module observations and commands modules, owns the exact state enum from the spec, and preserves RAM state across ordinary/fault sleep. Main supplies handles and foreground execution in Task 9.

- [ ] Write startup/wake cases: PG sequencing; cold-only inference at fault threshold minus 2 degrees; low-battery precedence; dummy settings reload on ordinary wake; no automatic activation; short wake press finishes validation; qualified waking press enables after readiness; the same press cannot disable.
- [ ] Add normal-mode cases: input insertion/removal preserves session; click boundaries cycle levels; 1 s fresh press disables; zero-level inactivity and entire-session deadlines; cold/hot faults latch; current derating changes live; heat waits for requested vibration calibration/restoration, while heat-only does not calibrate.
- [ ] Add battery cases: equality rules; input grants 10 s start/restart deadline; reported charge phase cancels it; expiry selects notice/shipping; crossing low threshold ends normal operation even while charging and requires a fresh enable after recovery. Notice preserves charging/SYS_ON for Charging, disables SYS_ON for Sleep; USB loss during notice/recovery restores shutdown policy. Evaluate all timers across rollover.
- [ ] Add fault cases: assigned code/priority, first fault retained, SYS_ON disabled, 60 s initial display, retained fault sleep, 10 s display on either wake source with no sensing, ignored 1 s gesture, and reboot clearing RAM latch. Do not emulate the external 10 s hardware reset in software.
- [ ] Run `./tests/runtime/run.sh app`; confirm failure. Implement the single-state transition logic using module mechanisms; prevent late enable requests from reviving a disabled/faulted session. Transition destinations must be reevaluated when input or faults change.
- [ ] Run the selector, all host tests and Debug build; commit. Record uncovered hardware prerequisites rather than treating fake readiness as production readiness.

### Task 9: Main integration, low power and hardware acceptance

**Files:** Modify only matching USER CODE blocks in `Massage_pen_FW/Core/Src/main.c`; complete `User/{Src/power.c,Inc/power.h}` and callback wiring in the owning user sources. Implement `User/{Inc/watchdog.h,Src/watchdog.c}` only after watchdog selection. Add new production sources explicitly to `User/CMakeLists.txt` if required.

**Interfaces:** Main initializes HAL peripherals, passes handles to app and dispatches foreground work on SysTick timing. User callbacks route events to the owning modules. Power enters the selected RAM-retaining sleep and restores required clocks/peripherals before ordinary wake validation; fault wake follows its separate path.

- [ ] Verify the prerequisite regenerated configuration and board decisions; compare generated changes against the user's regeneration. Do not edit protected files to work around missing DMA/wake settings.
- [ ] Add integration host cases for callback routing, no operation before initialization, and missed/overlapping ADC acquisitions counted without duplicate triggers. Do not catch up missed loop ticks by issuing back-to-back ADC frames and calling them 1 ms samples.
- [ ] Connect startup/dispatcher and callbacks, keep all app logic in User code, and connect the agreed sleep/watchdog mechanism. Before sleeping, quiesce transfers/output mechanisms and manage charger watchdog according to policy.
- [ ] Run `./tests/runtime/run.sh`, `./Massage_pen_FW/build.sh` and `git diff --check`. Verify main.c changes are confined to matching user blocks with unchanged line endings; no build artifacts or protected edits are staged.
- [ ] On hardware, measure 1 ms acquisition completion and app execution budget, half/full callback order, channel pairing, failure timeouts and PWM/LED behaviour. Verify battery shutdown and USB transitions, PG loss, short/long waking presses, RAM retention and both wake sources, fault wake without SYS_ON, and the external 10 s reset with relevant power sources.
- [ ] Validate sensor electrical limits and motor calibration; tune heater gains with actual temperature/power traces at each target, current ceiling and battery range. Verify inhibit/fault thresholds and overshoot behaviour. Do not mark hardware acceptance complete until battery-temperature monitoring and charger configuration are resolved.
- [ ] Commit integration after available checks pass; report exactly which host/build/hardware checks ran and which prerequisites remain unmet.

## Execution handoff

Review this plan before implementation. Native execution is recommended because
module contracts and app transitions are closely coupled; delegated execution is
available if explicitly selected. Choose the execution method after reviewing the
plan. Hardware-dependent tasks remain gated by the prerequisites above.
