#!/usr/bin/env sh
set -eu

selector=${1:-all}
runtime_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$runtime_dir/../.." && pwd)
build_dir=$(mktemp -d "${TMPDIR:-/tmp}/massage-pen-fw-runtime.XXXXXX")
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM

case "$selector" in
    power|sensors|i2c|ui|heater|vibration|charging|app|integration|all) ;;
    *) echo "unknown test selector: $selector" >&2; exit 2 ;;
esac

if [ "$selector" = power ] || [ "$selector" = all ]; then
cc -std=c11 -Wall -Wextra -Werror \
    -I"$runtime_dir/include" \
    -I"$runtime_dir" \
    -I"$repo_dir/Massage_pen_FW/Core/Inc" \
    -I"$repo_dir/Massage_pen_FW/User/Inc" \
    "$runtime_dir/test_power.c" \
    "$runtime_dir/fake_hal.c" \
    "$repo_dir/Massage_pen_FW/User/Src/power.c" \
    -o "$build_dir/test_power"
"$build_dir/test_power"
fi
if [ "$selector" = sensors ] || [ "$selector" = all ]; then
cc -std=c11 -Wall -Wextra -Werror \
    -I"$runtime_dir/include" -I"$runtime_dir" \
    -I"$repo_dir/Massage_pen_FW/Core/Inc" \
    -I"$repo_dir/Massage_pen_FW/User/Inc" \
    "$runtime_dir/test_sensors.c" "$runtime_dir/fake_hal.c" \
    "$runtime_dir/fake_adc.c" \
    "$repo_dir/Massage_pen_FW/User/Src/sensors.c" \
    -o "$build_dir/test_sensors"
"$build_dir/test_sensors"
fi

if [ "$selector" = i2c ] || [ "$selector" = all ]; then
cc -std=c11 -Wall -Wextra -Werror \
    -I"$runtime_dir/include" -I"$runtime_dir" \
    -I"$repo_dir/Massage_pen_FW/Core/Inc" -I"$repo_dir/Massage_pen_FW/User/Inc" \
    "$runtime_dir/test_i2c.c" "$runtime_dir/fake_hal.c" "$runtime_dir/fake_i2c.c" \
    "$repo_dir/Massage_pen_FW/User/Src/i2c_device.c" \
    "$repo_dir/Massage_pen_FW/User/Src/mp2724.c" \
    "$repo_dir/Massage_pen_FW/User/Src/drv2624.c" -o "$build_dir/test_i2c"
"$build_dir/test_i2c"
fi

if [ "$selector" = ui ] || [ "$selector" = all ]; then
cc -std=c11 -Wall -Wextra -Werror \
 -I"$runtime_dir/include" -I"$runtime_dir" \
 -I"$repo_dir/Massage_pen_FW/Core/Inc" -I"$repo_dir/Massage_pen_FW/User/Inc" \
 "$runtime_dir/test_ui.c" "$runtime_dir/fake_hal.c" \
 "$repo_dir/Massage_pen_FW/User/Src/buttons.c" \
 "$repo_dir/Massage_pen_FW/User/Src/leds.c" \
 "$repo_dir/Massage_pen_FW/User/Src/storage.c" -o "$build_dir/test_ui"
"$build_dir/test_ui"
fi

if [ "$selector" = heater ] || [ "$selector" = all ]; then
cc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all \
 -I"$runtime_dir/include" -I"$runtime_dir" \
 -I"$repo_dir/Massage_pen_FW/Core/Inc" -I"$repo_dir/Massage_pen_FW/User/Inc" \
 "$runtime_dir/test_heater.c" \
 "$repo_dir/Massage_pen_FW/User/Src/heater.c" \
 "$repo_dir/Massage_pen_FW/User/Src/pid.c" -o "$build_dir/test_heater"
"$build_dir/test_heater"
fi

if [ "$selector" = vibration ] || [ "$selector" = all ]; then
cc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all \
 -I"$runtime_dir/include" -I"$runtime_dir" \
 -I"$repo_dir/Massage_pen_FW/Core/Inc" -I"$repo_dir/Massage_pen_FW/User/Inc" \
 "$runtime_dir/test_vibration.c" "$runtime_dir/fake_hal.c" \
 "$repo_dir/Massage_pen_FW/User/Src/i2c_device.c" \
 "$repo_dir/Massage_pen_FW/User/Src/drv2624.c" \
 "$repo_dir/Massage_pen_FW/User/Src/vibration.c" -o "$build_dir/test_vibration"
"$build_dir/test_vibration"
fi

if [ "$selector" = charging ] || [ "$selector" = all ]; then
cc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all \
 -I"$runtime_dir/include" -I"$runtime_dir" \
 -I"$repo_dir/Massage_pen_FW/Core/Inc" -I"$repo_dir/Massage_pen_FW/User/Inc" \
 "$runtime_dir/test_charging.c" "$runtime_dir/fake_hal.c" \
 "$repo_dir/Massage_pen_FW/User/Src/i2c_device.c" \
 "$repo_dir/Massage_pen_FW/User/Src/mp2724.c" \
 "$repo_dir/Massage_pen_FW/User/Src/charging.c" -o "$build_dir/test_charging"
"$build_dir/test_charging"
fi

if [ "$selector" = app ] || [ "$selector" = all ]; then
cc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all \
 -I"$runtime_dir/include" -I"$runtime_dir" \
 -I"$repo_dir/Massage_pen_FW/Core/Inc" -I"$repo_dir/Massage_pen_FW/User/Inc" \
 "$runtime_dir/test_app.c" "$runtime_dir/fake_app_modules.c" "$runtime_dir/fake_hal.c" \
 "$repo_dir/Massage_pen_FW/User/Src/app.c" \
 "$repo_dir/Massage_pen_FW/User/Src/buttons.c" \
 "$repo_dir/Massage_pen_FW/User/Src/leds.c" -o "$build_dir/test_app"
"$build_dir/test_app"
cc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all \
 -I"$runtime_dir/include" -I"$runtime_dir" \
 -I"$repo_dir/Massage_pen_FW/Core/Inc" -I"$repo_dir/Massage_pen_FW/User/Inc" \
 "$runtime_dir/test_app_gating.c" "$runtime_dir/fake_hal.c" \
 "$runtime_dir/fake_i2c.c" "$runtime_dir/fake_adc.c" \
 "$repo_dir/Massage_pen_FW/User/Src/app.c" \
 "$repo_dir/Massage_pen_FW/User/Src/power.c" "$repo_dir/Massage_pen_FW/User/Src/sensors.c" \
 "$repo_dir/Massage_pen_FW/User/Src/i2c_device.c" \
 "$repo_dir/Massage_pen_FW/User/Src/mp2724.c" "$repo_dir/Massage_pen_FW/User/Src/charging.c" \
 "$repo_dir/Massage_pen_FW/User/Src/drv2624.c" "$repo_dir/Massage_pen_FW/User/Src/vibration.c" \
 "$repo_dir/Massage_pen_FW/User/Src/heater.c" "$repo_dir/Massage_pen_FW/User/Src/pid.c" \
 "$repo_dir/Massage_pen_FW/User/Src/buttons.c" "$repo_dir/Massage_pen_FW/User/Src/leds.c" \
 "$repo_dir/Massage_pen_FW/User/Src/storage.c" -o "$build_dir/test_app_gating"
"$build_dir/test_app_gating"
fi

if [ "$selector" = integration ] || [ "$selector" = all ]; then
cc -std=c11 -Wall -Wextra -Werror \
 -I"$runtime_dir/include" -I"$runtime_dir" \
 -I"$repo_dir/Massage_pen_FW/Core/Inc" -I"$repo_dir/Massage_pen_FW/User/Inc" \
 "$runtime_dir/test_runtime.c" "$runtime_dir/fake_hal.c" \
 "$repo_dir/Massage_pen_FW/User/Src/runtime.c" \
 "$repo_dir/Massage_pen_FW/User/Src/power.c" -o "$build_dir/test_runtime"
"$build_dir/test_runtime"
fi

if [ "$selector" = integration ] || [ "$selector" = all ]; then
cc -std=c11 -Wall -Wextra -Werror -DRUNTIME_HW_TEST \
 -I"$runtime_dir/include" -I"$runtime_dir" \
 -I"$repo_dir/Massage_pen_FW/Core/Inc" -I"$repo_dir/Massage_pen_FW/User/Inc" \
 "$runtime_dir/test_firmware_hw.c" "$runtime_dir/fake_hal.c" "$runtime_dir/fake_hardware.c" \
 "$repo_dir/Massage_pen_FW/User/Src/firmware_hw.c" \
 "$repo_dir/Massage_pen_FW/User/Src/sensors.c" \
 "$repo_dir/Massage_pen_FW/User/Src/i2c_device.c" \
 "$repo_dir/Massage_pen_FW/User/Src/mp2724.c" "$repo_dir/Massage_pen_FW/User/Src/drv2624.c" \
 -o "$build_dir/test_firmware_hw"
"$build_dir/test_firmware_hw"
fi

if [ "$selector" = integration ] || [ "$selector" = all ]; then
cc -std=c11 -Wall -Wextra -Werror -DRUNTIME_HW_TEST \
 -I"$runtime_dir/include" -I"$runtime_dir" \
 -I"$repo_dir/Massage_pen_FW/Core/Inc" -I"$repo_dir/Massage_pen_FW/User/Inc" \
 "$runtime_dir/test_runtime_policy.c" "$runtime_dir/fake_hal.c" "$runtime_dir/fake_hardware.c" \
 "$repo_dir/Massage_pen_FW/User/Src/runtime.c" "$repo_dir/Massage_pen_FW/User/Src/firmware_hw.c" \
 "$repo_dir/Massage_pen_FW/User/Src/app.c" "$repo_dir/Massage_pen_FW/User/Src/power.c" \
 "$repo_dir/Massage_pen_FW/User/Src/sensors.c" "$repo_dir/Massage_pen_FW/User/Src/i2c_device.c" \
 "$repo_dir/Massage_pen_FW/User/Src/mp2724.c" "$repo_dir/Massage_pen_FW/User/Src/charging.c" \
 "$repo_dir/Massage_pen_FW/User/Src/drv2624.c" "$repo_dir/Massage_pen_FW/User/Src/vibration.c" \
 "$repo_dir/Massage_pen_FW/User/Src/heater.c" "$repo_dir/Massage_pen_FW/User/Src/pid.c" \
 "$repo_dir/Massage_pen_FW/User/Src/buttons.c" "$repo_dir/Massage_pen_FW/User/Src/leds.c" \
 "$repo_dir/Massage_pen_FW/User/Src/storage.c" -o "$build_dir/test_runtime_policy"
"$build_dir/test_runtime_policy"
fi
