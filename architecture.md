# Firmware context

The STM32 CMake project is in `Massage_pen_FW/`. User code lives in `Massage_pen_FW/User/Inc` and
`Massage_pen_FW/User/Src`; `main.c` remains a minimal startup entry point.
The app owns raw battery charge admission and undervoltage policy; charging owns
the verified inhibit/current/lock sequence. Runtime mechanisms and application
policy are implemented in the `app`, `charging`,
`vibration`, `buttons`, `leds`, `sensors`, `power`, `heater`, `pid`,
and `storage` modules, with separate `mp2724` and `drv2624` drivers. Their APIs are
in the matching user headers. The [runtime dispatcher](Massage_pen_FW/User/Src/runtime.c)
and [hardware adapters](Massage_pen_FW/User/Src/firmware_hw.c) connect startup and
peripherals. The current sleep uses GPIO polling while retaining RAM; true MCU
low-power entry and physical acceptance remain pending.
HAL handles are passed explicitly from `main.c`; values belong in `#define`
macros in the relevant headers. The [runtime design](docs/superpowers/specs/2026-10-04-runtime-architecture-design.md)
records agreed behaviour and module ownership; it supersedes earlier runtime notes
in the topic documents. Startup currently passes NULL profiles, so policy gates
remain active. Settings loading currently returns dummy levels; there is no flash
save path. The MCU watchdog module is a stub; selection remains open.

Math rule: use integer or fixed-point arithmetic in firmware, with voltage in
mV, temperature in mdegC, and resistance in mΩ where needed. The MCU has no FPU.
Use sufficiently wide intermediate values to avoid overflow. Any runtime
floating-point exception requires agreement; host generation scripts may use it.

Read `AGENTS.md` for editing rules. Use the tiers below to load only relevant context:

- **Tier 1 — references:** [generated files](docs/architecture/generated.md)
  and [build](docs/architecture/build.md), when needed.
- **Tier 2 — requirements:** select the topic for the current task below.

| Topic | Document |
| --- | --- |
| Application policy and states | [Application](docs/architecture/application.md) |
| DRV2624 vibration control | [Vibration](docs/architecture/vibration.md) |
| MP2724GRH charging and battery reading | [Charging](docs/architecture/charging.md) |
| Heater PID and temperature reading | [Heater](docs/architecture/heater.md) |
| System power and wake | [Power](docs/architecture/power.md) |
| Three buttons and eight LED outputs | [UI](docs/architecture/ui.md) |
| Settings loading | [Storage](docs/architecture/storage.md) |
