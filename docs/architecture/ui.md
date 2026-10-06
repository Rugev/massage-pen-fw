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

Charging while normal uses the normal-operation display. Detailed charging-only
LED policy remains unresolved; the current app requests LEDs off in charging and
recovery states. Pins and ports for all buttons/LEDs come from
[main.h](../../Massage_pen_FW/Core/Inc/main.h); consult the read-only
[.ioc](../../Massage_pen_FW/Massage_pen_FW.ioc) for generated configuration.
