#ifndef USER_LEDS_H
#define USER_LEDS_H
#include <stdbool.h>
#include <stdint.h>
#define LEDS_ACTIVE_HIGH 1U
#define LEDS_MAX_LEVEL 3U
#define LEDS_BREATHE_PERIOD_MS 2000U
#define LEDS_MODULATION_STEPS 32U
/* Keep coprime with the modulation step count. */
#define LEDS_MODULATION_STRIDE 13U
#define LEDS_NOTICE_HALF_PERIOD_MS 500U
#define LEDS_NOTICE_FLASHES 3U
#define LEDS_FAULT_HALF_PERIOD_MS 100U
#define LEDS_PROGRESS_HALF_PERIOD_MS 500U
#define LEDS_PROGRESS_MIDDLE_MV 3600U
#define LEDS_PROGRESS_HIGH_MV 3900U
typedef enum { LEDS_OFF, LEDS_LEVELS, LEDS_LOW_BATTERY, LEDS_FAULT, LEDS_CHARGING } Leds_Mode;
typedef enum { LEDS_BATTERY_OFF, LEDS_BATTERY_GREEN, LEDS_BATTERY_BREATHE } Leds_BatteryPattern;
typedef struct {
 Leds_Mode mode;
 uint8_t heat_level, vibration_level;
 bool heater_breathe;
 Leds_BatteryPattern battery_pattern;
 /* 0: no progress; 1..3: solid preceding LEDs and blink this LED. */
 uint8_t charging_progress;
 uint8_t fault_code;
} Leds_Display;
void Leds_Init(uint32_t now);
/* Unchanged requests preserve phase. Change mode away and back to start a new notice. */
void Leds_Request(const Leds_Display *display, uint32_t now);
/* Call once per 1 ms foreground slot; bounded software modulation, no delays. */
void Leds_Update(uint32_t now);
bool Leds_NoticeComplete(uint32_t now);
#endif
