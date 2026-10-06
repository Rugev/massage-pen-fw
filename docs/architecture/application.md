# Application policy

[app.c](../../Massage_pen_FW/User/Src/app.c) owns state transitions, requested
levels, session timers, battery policy and the first latched fault. The complete
state enum, timing constants and public API are in
[app.h](../../Massage_pen_FW/User/Inc/app.h).

| State group | Implemented policy |
| --- | --- |
| Startup / ordinary wake | Enable `SYS_ON`, await power good and measurements, validate charger status/configuration, then load settings and select operation, charging or sleep. Only cold boot applies inferred overtemperature checking; low battery takes precedence. |
| Normal / charging while normal | Permit requested outputs through module interlocks. Input insertion/removal changes state without restarting the session. Shutdown or session expiry selects charging with input, otherwise sleep. |
| Charging / charging recovery | Inhibit normal outputs. Low battery with valid input gets a bounded wait; a fresh admitted active phase cancels the deadline, and later inactivity starts another. |
| Low-battery notice / battery disconnect | End normal operation at or below the standby threshold; voltage recovery does not restart it. Show the notice before charging or sleep, or request shipping immediately at or below the disconnect threshold when shutdown policy applies. |
| Sleep | Disable `SYS_ON`; complete shutdown work before entering RAM-retaining polling sleep. |
| Fault display / fault sleep | Disable `SYS_ON` and retain the first fault until reboot. Fault wake briefly redisplays the retained code without ordinary validation or sensing. |

A fresh current-valid raw battery sample independently gates charging admission;
undervoltage below that admission threshold latches its own fault before battery
shutdown policy runs. This fault remains latched across USB changes and RAM sleep/wake
until reboot. The app publishes charger demand and eligibility before
charger sequencing and again after same-cycle state changes. See [charging](charging.md)
and [app.c](../../Massage_pen_FW/User/Src/app.c).

Enable requires a qualifying power press, power good, valid measurements, battery
above the standby threshold, charger configuration/status readiness and a valid
motor profile when vibration is requested. Loading settings alone never enables
outputs. Heater authorization also waits for motor configuration readiness;
nonzero vibration additionally requires calibration/restoration readiness.
Settings currently come from the dummy loader in
[storage.c](../../Massage_pen_FW/User/Src/storage.c). Battery thresholds are in
[power.h](../../Massage_pen_FW/User/Inc/power.h); gestures and displays are in
[UI](ui.md).

[main.c](../../Massage_pen_FW/Core/Src/main.c) passes peripheral handles to
[runtime.c](../../Massage_pen_FW/User/Src/runtime.c), which dispatches once per
distinct HAL millisecond tick and records missed ticks without replaying them.
App calls the module owners in foreground; interrupt adapters capture completion
evidence. Foreground updates continue while ADC and bus cancellation drains and
charger sleep preparation remain pending. Runtime polls wake edges throughout
operation and queues edges arriving during shutdown for the sleep handoff; see
[Power](power.md).

Startup currently passes `NULL` runtime profiles. No approved charger/motor
profile or charger idle-register lease is supplied, so configuration readiness,
ordinary validation completion and normal output operation remain gated; low
battery shutdown and fault handling can still run. Public bindings are in
[runtime.h](../../Massage_pen_FW/User/Inc/runtime.h),
[charging.h](../../Massage_pen_FW/User/Inc/charging.h) and
[vibration.h](../../Massage_pen_FW/User/Inc/vibration.h).

The [runtime requirements](../superpowers/specs/2026-10-04-runtime-architecture-design.md)
record agreed policy and fault priority. Hardware acceptance, battery-only
temperature-status freshness and the remaining profiles/settings remain open.
