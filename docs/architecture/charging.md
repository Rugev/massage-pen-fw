# Charging

Communicate with the MP2724GRH battery charger over I2C and read analog
battery voltage. Charging mode enables `SYS_ON`, monitors the charger and
battery, and displays charge on LEDs. Hybrid mode also operates heat/vibration.
The charger interrupt is a wake source in Standby.

Use `CHRG_I2C_SCL_Pin`, `CHRG_I2C_SDA_Pin`, `CHRG_INT_Pin` and `VBATT_SENS_Pin`
from [main.h](../../Massage_pen_FW/Core/Inc/main.h). Consult the read-only
[.ioc](../../Massage_pen_FW/Massage_pen_FW.ioc) for configuration.
Skeletons: `Massage_pen_FW/User/Src/charging.c` and separate driver `Massage_pen_FW/User/Src/mp2724.c`,
with matching headers in `Massage_pen_FW/User/Inc`. APIs and implementation remain undecided.
IC operation: [MP2724 functional reference](mp2724.md); register definitions and
encodings are in [mp2724.h](../../Massage_pen_FW/User/Inc/mp2724.h).

Battery: 1S Li-ion. Voltage range, nominal voltage and maximum charging current
are configurable in `Massage_pen_FW/User/Inc/charging.h`; address is in `Massage_pen_FW/User/Inc/mp2724.h`.
Battery voltage is twice the ADC input voltage; the configurable multiplier
is in `Massage_pen_FW/User/Inc/sensors.h`.

Request complete battery disconnection through the charger at
`BATTERY_DISCONNECT_MV` in `Massage_pen_FW/User/Inc/power.h`; the higher `BATTERY_STANDBY_MV`
requests Standby only. The disconnect command remains to be implemented.

## Agreed configuration

Operating setpoints and cell limits are in
[charging.h](../../Massage_pen_FW/User/Inc/charging.h), separate from the IC's
factory defaults. Follow USB source detection and cap input at
`CHARGER_USB_INPUT_MAX_MA`; keep the separate battery charging limit.
Continue charging in Hybrid mode when input power permits; system demand takes
priority and may reduce charging or require battery supplementation.

Pre-charge settings and disabled trickle charging are provisional: check both
with the cell manufacturer, including permitted deep-discharge recovery.
The NTC1 thermistor is positioned near the battery centre. Apply the configured
cold cutoff and cool current reduction. Set the autonomous hot cutoff using
`CHARGER_HOT_THRESHOLD_MDEGC` as a fallback; it does not replace the MCU cutoff
or extend the cell's permitted charging range. The MCU pauses charging at the warm
threshold (also handling hot/missing-NTC conditions); resume only after cooling
below it, when charging is required and other protections permit. Retain charge
completion state across a temperature pause so clearing `EN_CHG` does not cause
an unnecessary restart. NTC status includes comparator hysteresis; it is not
an exact temperature measurement. This warm cutoff depends on the MCU and does
not provide autonomous protection at the cell's upper charging limit. Battery-only
temperature monitoring for discharge derating remains unresolved: VRNTC is
documented as powered during buck/boost operation, and boost is disabled.

Enable/service the charger watchdog while USB input is valid, including
temperature pauses and waiting for automatic recharge after completion. Disable it
on battery-only operation or before sleep. Check every documented configuration
register during recovery, handling commands, reserved bits and automatically
updated fields separately. Watchdog expiry can restore less restrictive NTC
thresholds; recovery must re-establish the configured protection.

Keep USB boost disabled, enable RST system power cycling, request immediate
shipping entry, lock charging parameters after configuration/readback, and
unmask all interrupt events. Preserve reserved bits in `INT_MASK`.

Use `CHARGER_SYS_MIN_MV` for the minimum SYS setting. Die thermal regulation
threshold, charge safety timeout, recharge/top-off policy and remaining
register settings still require agreement. No
initialization or runtime control is implemented by these definitions.

## Charging by battery temperature

Thresholds and setpoints below are defined in [charging.h](../../Massage_pen_FW/User/Inc/charging.h).
Ranges describe nominal transitions; NTC hysteresis affects recovery boundaries.

| Battery temperature | Planned behavior |
| --- | --- |
| Below `CHARGER_COLD_THRESHOLD_MDEGC` | IC suspends charging. |
| From `CHARGER_COLD_THRESHOLD_MDEGC` to below `CHARGER_COOL_THRESHOLD_MDEGC` | Cool mode limits fast-charge current to `CHARGER_COOL_CURRENT_PERCENT` of `BATTERY_MAX_CHARGE_CURRENT_MA`. |
| From `CHARGER_COOL_THRESHOLD_MDEGC` to below `CHARGER_WARM_PAUSE_THRESHOLD_MDEGC` | Normal charging, up to `BATTERY_MAX_CHARGE_CURRENT_MA`, targeting `BATTERY_MAX_MV`. |
| From `CHARGER_WARM_PAUSE_THRESHOLD_MDEGC` to below `CHARGER_HOT_THRESHOLD_MDEGC` | MCU pauses charging; resume after the warm indication clears, only if charging is required and other protections permit. |
| At or above `CHARGER_HOT_THRESHOLD_MDEGC` | IC suspends charging as the hardware fallback. |

The hardware hot fallback exceeds `BATTERY_CHARGE_MAX_MDEGC`; it does not
permit charging above the cell limit or replace the MCU warm cutoff. Watchdog
expiry can restore factory NTC thresholds.

## Charging by battery voltage

This profile applies only when temperature and other protections allow charging.
Input power, system demand and die thermal regulation can reduce actual current;
constant-voltage charging also tapers current below the applicable ceiling.

| Battery voltage / phase | Planned behavior |
| --- | --- |
| Below the IC's fixed trickle-to-pre-charge threshold (approximately 2V) | Trickle phase uses `CHARGER_TRICKLE_CURRENT_MA`, currently disabled; no recovery charging. |
| From the fixed trickle threshold to below `CHARGER_PRECHARGE_THRESHOLD_MV` | Pre-charge at `CHARGER_PRECHARGE_CURRENT_MA`. |
| From `CHARGER_PRECHARGE_THRESHOLD_MV` until the regulation target is reached | Constant-current charging up to the temperature-dependent fast-charge ceiling. |
| At the `BATTERY_MAX_MV` regulation target | Constant-voltage charging holds the target while current tapers. |
| In constant-voltage mode, current below `BATTERY_TERMINATION_CURRENT_MA` | Terminate after the IC's debounce, provided termination is enabled and input/thermal regulation does not block it. |

Voltage transitions have hardware hysteresis. Pre-charge and disabled trickle
charging await manufacturer confirmation. `BATTERY_MIN_MV` is the cell discharge
endpoint, separate from the charge-phase thresholds and system power cutoffs.
Recharge offset and top-off behavior remain undecided.
