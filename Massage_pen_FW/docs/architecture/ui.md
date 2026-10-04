# Buttons and LEDs

Read three button inputs and control eight LED outputs. Charging displays
battery charge; Normal operation monitors buttons and sets LEDs.

Use definitions from [main.h](../../Core/Inc/main.h):

- Buttons: `BUTTON_PWR_ON_Pin`, `BUTTON_VIBRATION_Pin`, `BUTTON_HEAT_Pin`.
- Battery LEDs: `BAT_LED_R_Pin`, `BAT_LED_G_Pin`.
- Heat LEDs: `LED_HEAT_1_Pin`, `LED_HEAT_2_Pin`, `LED_HEAT_3_Pin`.
- Vibration LEDs: `LED_VIBRATION_1_Pin`, `LED_VIBRATION_2_Pin`, `LED_VIBRATION_3_Pin`.

Consult the read-only [.ioc](../../Massage_pen_FW.ioc) for configuration.
Button gestures, LED patterns and implementation structure will be decided later.

LEDs and buttons are active high, configurable in `User/Inc/leds.h` and
`User/Inc/buttons.h`. Heat/vibration levels 0..3 correspond to 0..3 LEDs lit.
