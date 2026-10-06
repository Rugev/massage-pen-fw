# Application-owned build integration; STM32CubeMX does not manage this file.
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
)

# Register application sources explicitly.
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    Src/app.c
    Src/runtime.c
    Src/firmware_hw.c
    Src/charging.c
    Src/i2c_device.c
    Src/mp2724.c
    Src/vibration.c
    Src/drv2624.c
    Src/buttons.c
    Src/leds.c
    Src/sensors.c
    Src/power.c
    Src/heater.c
    Src/pid.c
    Src/watchdog.c
    Src/storage.c
)
