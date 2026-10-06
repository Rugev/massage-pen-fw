# Charging Admission Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` for native execution or `superpowers:subagent-driven-development` if the user selects delegation. Execute task by task using the checkboxes below, after plan review.

**Goal:** Implement reviewed charging admission, latched undervoltage, controlled pre-charge and charging/battery LED indications.

**Architecture:** App owns battery/fault policy and normal-operation authorization. Charging owns inhibition, phase-dependent current, lock sequencing and configuration expectations through the existing MP2724 transport. Sensors supplies current unfiltered battery data independently of retained control readings.

**Tech Stack:** STM32 HAL, C, integer arithmetic, asynchronous I2C, host C tests with fake peripherals, prescribed Debug build.

**Spec:** [Charging admission requirements](../specs/2026-10-06-charging-admission-design.md), supplementing the [runtime design](../specs/2026-10-04-runtime-architecture-design.md).

## Global Constraints

- Follow [AGENTS.md](../../../AGENTS.md) and [architecture.md](../../../architecture.md). User files only; no protected/generated-file changes are planned.
- New hardware/tunable values belong in relevant user headers. Preserve integer math, generated pins and explicit HAL handles.
- Preserve the separate cell endpoint and application standby/disconnect thresholds, temperature protections, USB ceiling, completion/top-off policy and watchdog service during warm pauses/completion.
- Exactly the admission threshold passes its voltage check; no added margin or filter delay. The IC owns its nominal pre-charge transition and hysteresis.
- Pre-charge remains zero during both normal-operation states, regardless of requested levels; fast/CV charging may continue when otherwise eligible.
- Automatic pre-charge revalidation is allowed; fault recovery and a new post-completion recharge policy are not.
- Battery error/charging indications take priority independently of heat/vibration level displays; normal operation retains heat/vibration display priority.
- Keep NULL startup profiles and unresolved production profile/idle lease gates. Do not turn test profiles into board settings.
- Build firmware only with `./Massage_pen_FW/build.sh`; keep artifacts untracked.
- Public API signatures remain open under repository guidance. Interface blocks below define required data/ownership; agree concrete declarations before implementing their task. Fault display allocation likewise remains a review decision.

## Review Focus

- A valid low battery sample with a missing tip sample must still detect undervoltage: Task 1.
- A canceled write may already have changed enable/current/lock on the IC: Tasks 2 and 4.
- Initial zero-current charging must expose pre-charge before current can be raised: Tasks 3 and 6.
- Normal-operation entry or USB loss during arming must prevent a late enable/current increase: Tasks 3–4.
- IRQ floods and long cancellation drains must not hide phase/fault updates or starve watchdog service: Tasks 3–4 and 6.
- LED level/band changes must not restart independent battery animation, and stale status must not imply charging: Task 5.

## Sequence to implement and review

1. Establish verified charging inhibition and zero pre-charge before ordinary configuration validation, even with a missing/invalid profile or unavailable idle lease. Keep buck available for power and NTC qualification.
2. Once fresh battery and all other admission checks pass, permit charging with pre-charge still zero. Observe a fresh `CHG_STAT`; unknown, not-charging, trickle, fast, CV and done phases do not permit operating pre-charge current.
3. On observed pre-charge outside normal operation, obtain a fresh qualifying battery reading after that phase observation. Verify charging off, unlock, write/verify operating pre-charge, relock/verify and enable only if eligibility remains valid. This controlled arming sequence temporarily programs operating current before enable; ordinary inhibited/paused states require zero.
4. On departure from pre-charge, normal-operation entry, USB loss, pause or loss of eligibility/phase freshness, request zero and revoke arming. Fast/CV/done alone requires current reduction, not a charging-enable toggle that would restart the cycle. Invalid sensing/fault/thermal pause also requires charging inhibition.
5. A later pre-charge observation automatically requests fresh revalidation. Reuse the safe sequence without user action, unless fault/completion policy prohibits restart.

Do not infer pre-charge from an ADC upper-voltage cutoff. Confirm the IC reports pre-charge with zero current; if it cannot, retain zero and report the missing hardware/manufacturer contract rather than arm on unknown phase. During deliberate inhibition for arming, a resulting not-charging status is not a new departure event; any actual fault/input/voltage revocation still aborts arming.

## Task 1: Battery freshness and app fault policy

**Files:** `Massage_pen_FW/User/{Inc/sensors.h,Src/sensors.c,Inc/app.h,Src/app.c,Inc/charging.h}`; `tests/runtime/{test_sensors.c,test_app.c,fake_app_modules.h,fake_app_modules.c}`.

**Interfaces:** Sensors publishes fresh battery-channel validity/value separately from pair freshness and retained `ready`. App consumes raw voltage, owns the undervoltage latch and supplies current charging eligibility plus normal-operation state. Agree the new fault code/priority without renumbering existing faults.

- [ ] Add tests for battery-only channel completion, missing battery with retained `ready`, settling, skipped slots and late callbacks. Assert current battery eligibility never comes from an old retained value, and missing tip data cannot hide a newly acquired low battery.
- [ ] Run `./tests/runtime/run.sh sensors`; verify the new freshness cases fail before implementation.
- [ ] Extend the sensor observation contract with independent battery freshness; retain existing filter, pair-conversion, heater retry and acquisition-fault behavior.
- [ ] Add app cases for raw readings 2499/2500/2501mV, including a higher filtered value, with and without USB in startup/operation. Assert only valid below-threshold readings latch undervoltage; invalid/missing readings inhibit charging and follow existing sensor fault rules. Verify fault retention through voltage recovery, USB cycling, fault wake and reboot.
- [ ] Add the admission threshold macro in `charging.h`; implement the agreed fault code/priority in app. Detect battery faults before ordinary low-battery recovery/shipping policy; keep all existing thresholds separate.
- [ ] Run `./tests/runtime/run.sh sensors` and `./tests/runtime/run.sh app`; verify new and existing cases pass. Commit only this task's files after verification.

## Task 2: Verified safe baseline and profile auditing

**Files:** `Massage_pen_FW/User/{Inc/charging.h,Src/charging.c}`; `tests/runtime/{test_charging.c,test_app_gating.c,test_runtime_policy.c}`.

**Interfaces:** Reviewed profile remains input; charging exposes safe-baseline readiness separately from configuration and charging permission. Reuse `MP2724_RequestConfig()` with field masks and verified results; no driver changes expected.

- [ ] Add tests starting with `EN_CHG` set and nonzero factory current, including NULL/invalid profile and refused IIN/CHG_CTRL3 lease. Assert inhibit readback precedes ordinary audit/unlock and admission stays false before verified zero current.
- [ ] Run `./tests/runtime/run.sh charging`; verify the baseline cases fail on current behavior.
- [ ] Split safe/operating pre-charge macros, update the nominal transition and enforce the reviewed profile targets from the spec. Preserve fast-charge/regulation/termination/trickle settings; reject obsolete profiles. Update synthetic profiles through header field encodings, retaining their explicit test-only status.
- [ ] Add the safe-baseline sequence and make audit expectations use current sequencer-owned `EN_CHG`/`IPRE` targets. Preserve reserved/action/unrelated fields; do not preserve an unsafe hardware enable as validation authorization. Ordinary audits cannot restore operating pre-charge after revocation.
- [ ] Invalidate cached hardware facts after possibly transmitted/canceled writes; reconcile after drain. Test an input-audit completion cannot restore readiness after an intervening fault or validation restart.
- [ ] Run `./tests/runtime/run.sh charging`, `./tests/runtime/run.sh app` and `./tests/runtime/run.sh integration`; verify all pass. Commit the task files.

## Task 3: Admission and automatic pre-charge sequencing

**Files:** `Massage_pen_FW/User/{Inc/charging.h,Src/charging.c,Src/app.c}`; `tests/runtime/{test_charging.c,test_app.c,fake_app_modules.h,fake_app_modules.c}`.

**Interfaces:** App supplies current battery eligibility, fault inhibition, charge demand and both normal-operation states before charging progresses. Charging owns phase freshness, revalidation generation, desired pre-charge current and unlock/write/relock/enable progress. Configuration readiness remains usable during battery-only validation.

- [ ] Extend the charger fake to trace writes/readbacks and enforce the lock's increase/reduction rules. Add cases for initial admitted enable at zero, observed pre-charge followed by a newer valid sample, and readback-verified unlock/current/relock before enable. Assert no operating current for unknown/trickle/not-charging/fast/CV/done phase or either normal-operation state.
- [ ] Run `./tests/runtime/run.sh charging`; verify the admission/sequence cases fail before implementation.
- [ ] Implement the reviewed sequence above with explicit verified stages. Eligibility revocation invalidates earlier permission; a completed older transaction cannot authorize a new cycle. Recheck latest eligibility before current increase and enable, without requiring a single ADC sample to remain unchanged throughout asynchronous work.
- [ ] Schedule zero-current reductions promptly after phase decode without waiting for another polling interval/full round. Preserve bounded round progression, NTC/fault freshness, watchdog servicing and input ceiling reconciliation under IRQ flood.
- [ ] Add return-to-pre-charge cases: no newer ADC keeps zero; newer eligible ADC automatically rearms; warm/fault/normal operation forbids rearm. Add first-observed fast/CV/done, unchanged-state no redundant write, dynamic audit and completion-retention cases.
- [ ] Move app commands/eligibility ahead of charger progression and reconcile transitions in the same app cycle. Test initial output inhibition and both normal-operation states with all levels zero as well as active levels. The existing recovery deadline must not accept an unadmitted autonomous charging phase as successful admission.
- [ ] Run `./tests/runtime/run.sh charging` and `./tests/runtime/run.sh app`; verify all pass. Commit the task files.

## Task 4: Revocation, faults and lifecycle

**Files:** Charging/app files from Task 3; `tests/runtime/{test_charging.c,test_app.c,test_app_gating.c,test_runtime_policy.c}`.

**Interfaces:** Inhibition/current/lock observations distinguish requested, verified and unknown state. Keep the existing transport exhaustion latch, drain ownership, fault retention and one-shot shipping contract.

- [ ] Inject invalid ADC, USB loss, normal entry, warm pause, fault, sleep and restart at every arming stage, including after physical writes before readback. Assert no late authorization, prompt zero target, reconciliation after drain and fresh revalidation before restart.
- [ ] Inject failed writes/readbacks/mismatches at inhibit, zero, unlock, operating current, relock and enable. Verify the existing three-attempt fault behavior and that unrelated success cannot erase failure history. Exhausted transport keeps permission revoked and physical inhibition unknown; never reinitialize it to bypass faults.
- [ ] Run `./tests/runtime/run.sh charging` and `./tests/runtime/run.sh app`; verify new cases fail before implementing lifecycle changes.
- [ ] Implement revocation/reconciliation and fault/sleep preparation that verifies inhibition and zero before reporting successful safety preparation. Drain work without replaying shipping commands; terminal failure must not falsely claim safety readiness.
- [ ] Preserve watchdog service during ordinary warm/done pauses, battery-only/sleep disable and startup/wake-only expiry recovery. Test MCU restart with USB/charging already active, reconnect after zeroing, runtime expiry, warm resume, completion and timer rollover. Phase polling progress must remain bounded under IRQ floods.
- [ ] Run `./tests/runtime/run.sh charging`, `./tests/runtime/run.sh app` and `./tests/runtime/run.sh integration`; verify all pass. Commit the task files.

## Task 5: Charging progress and independent battery indication

**Files:** `Massage_pen_FW/User/{Inc/leds.h,Src/leds.c,Src/app.c}`; `tests/runtime/{test_ui.c,test_app.c,test_runtime_policy.c}`. Include `Src/runtime.c` and runtime tests only if the user selects continuous red blinking in fault sleep.

**Interfaces:** App chooses heat/vibration display separately from battery indication. LEDs renders independently timed off/solid/blink/breathe patterns and retains fault/notice semantics. Concrete display declarations remain open; no charging policy moves into the renderer.

- [ ] Resolve the spec's completion/pause and fault-sleep questions before dependent implementation. Continuous fault-sleep blinking, if selected, requires display-only progression while sensing, SYS_ON and outputs remain off; it must not call ordinary app/wake validation just to animate LEDs.
- [ ] Add UI tests for the three charging-progress patterns at 3599/3600/3899/3900mV, one-second blink timing, green breathing and red error blink. Test independent heater/battery breathing and retained battery phase while levels/progress bands change, including tick rollover.
- [ ] Add app cases covering charging-only/recovery with fresh admitted status, normal without charging, normal plus charging, and error overriding green in each. Assert normal heat/vibration levels take precedence even when levels are zero; heat LEDs stay off for charging progress. Stale status, unavailable voltage, USB presence alone and zero-current unadmitted phases must not manufacture progress or a breathing charging indication.
- [ ] Test active top-off remains a charging indication despite done phase; retain existing six-bit fault output and low-battery notice. Add completion/pause/fault-sleep assertions matching the user's answers.
- [ ] Run `./tests/runtime/run.sh ui` and `./tests/runtime/run.sh app`; verify new pattern/priority tests fail before implementation.
- [ ] Implement display-only voltage boundaries and blink timing as user-header macros, using filtered valid voltage. Reuse bounded integer software modulation for green breathing. Split pattern phase ownership so unrelated display changes do not restart battery or heater animations.
- [ ] Update app display selection using the existing fault latch, normal-operation state and fresh admitted charger/top-off observations. A warm pause is not an error by itself. Preserve protection/admission math and existing GPIO definitions.
- [ ] Run `./tests/runtime/run.sh ui`, `./tests/runtime/run.sh app` and `./tests/runtime/run.sh integration`; require all pass. Commit only task files after verification.

## Task 6: Documentation, regression and hardware acceptance

**Files:** `docs/architecture/charging.md`, `docs/architecture/ui.md`, `docs/superpowers/specs/2026-10-04-runtime-architecture-design.md`; existing host tests if regression gaps emerge. Update `docs/architecture/mp2724.md` only for a confirmed change to the IC reference.

**Interfaces:** Documentation references implemented user-header values and APIs; it does not supply the unresolved production profile, idle lease or recharge policy.

- [ ] Update charging/UI requirements minimally and remove obsolete manufacturer-confirmation and charging-LED-open wording. Record the agreed undervoltage fault code/priority and any agreed fault-sleep display change in the runtime design. Link source for values; retain unrelated open items.
- [ ] Run `./tests/runtime/run.sh`, `./Massage_pen_FW/build.sh` and `git diff --check`; require all host suites, Debug build and whitespace checks to pass. Check protected files are unchanged and artifacts remain untracked; commit only reviewed task files.
- [ ] On hardware, verify pre-charge status visibility at zero current, actual safe/operating currents, lock increase/reduction behavior, nominal phase transition, CV/termination and output inhibition. Verify automatic return-to-pre-charge admission and USB/normal-operation zeroing.
- [ ] Measure phase/event-to-zero latency under polling, interrupts, load and retries, plus ADC settling/acquisition timing. Observe MCU restart, USB insertion and watchdog reset/expiry with actual battery current. Record any autonomous charging window; do not claim zero `IPRE` protects fast charge or failed transport.
- [ ] Inspect charging-progress blinking, simultaneous heater/green breathing, red error priority and agreed completion/pause/sleep behavior on hardware; confirm modulation leaves the foreground timing budget intact.
- [ ] Report actual host/build/hardware checks separately. Production charging remains gated until the complete profile, idle lease and hardware acceptance are supplied; report required protected-file/hardware changes rather than inventing them.

## Review and execution

Review the zero-current startup/phase-observation sequence, arming transition and fault display allocation before implementation. Resolve completion/pause indications and fault-sleep error blinking for Task 5. Concrete public declarations remain open as required by repository instructions. Native execution is recommended because app ordering, charger sequencing and cancellation share one lifecycle; delegation is available if explicitly selected. No implementation or hardware verification has been performed by writing this plan.
