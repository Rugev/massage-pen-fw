# Firmware context

The STM32 CMake project is in `Massage_pen_FW/`. User code lives in `Massage_pen_FW/User/Inc` and
`Massage_pen_FW/User/Src`; `main.c` remains a minimal startup entry point.
File skeletons exist for `app`, `charging`, `vibration`, `buttons`, `leds`,
`sensors`, `power`, `heater`, `pid`, `watchdog`, and `storage`, with separate `mp2724` and
`drv2624` drivers. APIs and implementation architecture remain undecided.
HAL handles will be passed explicitly from `main.c`; values belong in `#define`
macros in the relevant headers. Startup is not yet connected to user code.

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
| Four operating states | [Application](docs/architecture/application.md) |
| DRV2624 vibration control | [Vibration](docs/architecture/vibration.md) |
| MP2724GRH charging and battery reading | [Charging](docs/architecture/charging.md) |
| Heater PID and temperature reading | [Heater](docs/architecture/heater.md) |
| System power, wake, watchdogs | [Power](docs/architecture/power.md) |
| Three buttons and eight LED outputs | [UI](docs/architecture/ui.md) |
| Persist requested vibration and heat levels | [Storage](docs/architecture/storage.md) |
