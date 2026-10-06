# Buttons and LEDs

[buttons.c](../../Massage_pen_FW/User/Src/buttons.c) debounces press and release
edges and retains press identities/durations for
[app.c](../../Massage_pen_FW/User/Src/app.c) to interpret. The qualifying power
hold requests enable during validation or charging; a fresh shutdown hold ends
normal operation. One press cannot both enable and disable. A waking power press
counts after debounce; releasing early still allows validation to finish. Fault
states ignore ordinary gestures; the long hardware-reset hold is a charger
function.

Heat/vibration release clicks strictly between the click-duration limits cycle
requested levels only during normal operation. Session expiry ends the whole
session; level changes and input-power transitions do not reset it. Keeping both
requested levels zero also ends the session after its separate idle limit.
Gesture/session constants are in [app.h](../../Massage_pen_FW/User/Inc/app.h),
and debounce/polarity definitions are in
[buttons.h](../../Massage_pen_FW/User/Inc/buttons.h).

[leds.c](../../Massage_pen_FW/User/Src/leds.c) renders the app-requested display:
level LEDs indicate requested heat/vibration levels, with heat LEDs breathing
during preheat/precool and solid during PID operation. Low-battery notice flashes
the red battery LED; fault display flashes red while the heat/vibration outputs
show the solid fault-code bits, heat first. Identical display requests preserve
pattern phase. Pattern constants and polarity are in
[leds.h](../../Massage_pen_FW/User/Inc/leds.h); fault priority is defined by
[app.c](../../Massage_pen_FW/User/Src/app.c).

During normal operation or charging, fresh admitted active/top-off status with a
fresh valid battery sample breathes the battery LED. A fresh retained DONE phase
shows green even if admission was revoked by a warm pause. When normal operation
is not paused, the battery LED is solid green even if charging is not active. Warm
pause leaves it off; after cooling, retained completion returns it to green without
enabling charging. Charging-only progress is filtered battery voltage mapped to
the bands in [leds.h](../../Massage_pen_FW/User/Inc/leds.h). Normal-operation level
display takes precedence, including when both requested levels are zero; heater
preheat/precool breathe independently of the battery and progress patterns; level
changes preserve the active pattern phases. Heat LEDs are off in charging-only
mode. Low-battery notice and fault display have
their own phases; LEDs are off in fault sleep. Display policy is in
[app.c](../../Massage_pen_FW/User/Src/app.c) and rendering in
[leds.c](../../Massage_pen_FW/User/Src/leds.c).
Pins and ports for all buttons/LEDs come from
[main.h](../../Massage_pen_FW/Core/Inc/main.h); consult the read-only
[.ioc](../../Massage_pen_FW/Massage_pen_FW.ioc) for generated configuration.
