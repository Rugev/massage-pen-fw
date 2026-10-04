# Heater

Control the heater using PID, PWM output and thermistor feedback.
Heating is supported during Normal operation and Hybrid mode.

Use `TIP_HEAT_CTRL_Pin` for heater control and `TIP_HEAT_TEMP_SENS_Pin`
for feedback, from [main.h](../../Massage_pen_FW/Core/Inc/main.h). Consult the read-only
[.ioc](../../Massage_pen_FW/Massage_pen_FW.ioc) and generated code for configuration.
PID design, settings and safety behavior will be decided later.

The heater is an NMOS-switched resistor; control low means off. Power control
must account for measured battery voltage. Resistance, absolute maximum
temperature and four levels (off plus three targets) are in `Massage_pen_FW/User/Inc/heater.h`.
Temperature conversion must use a generated lookup table with integer linear
interpolation. Thermistor/divider values are in `Massage_pen_FW/User/Inc/sensors.h`.

The thermistor is nominal at 25°C. Its divider is supplied by battery voltage;
the ADC reference is fixed. Normalize the input voltage by measured battery
voltage before table lookup. Regenerate `Massage_pen_FW/User/Inc/thermistor_lut.h` with
`python3 Massage_pen_FW/scripts/generate_thermistor_lut.py` after changing sensor constants.
The table stores integer divider ratios and mdegC; generation bounds and spacing
are configurable in `sensors.h` and do not define operating temperature limits.
