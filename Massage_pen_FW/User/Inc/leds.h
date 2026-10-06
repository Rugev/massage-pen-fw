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
typedef enum { LEDS_OFF, LEDS_LEVELS, LEDS_LOW_BATTERY, LEDS_FAULT } Leds_Mode;
typedef struct {
 Leds_Mode mode;
 uint8_t heat_level, vibration_level;
 bool heater_breathe, battery_red, battery_green;
 uint8_t fault_code;
} Leds_Display;
void Leds_Init(uint32_t now);
/* Unchanged requests preserve phase. Change mode away and back to start a new notice. */
void Leds_Request(const Leds_Display *display, uint32_t now);
/* Call once per 1 ms foreground slot; bounded software modulation, no delays. */
void Leds_Update(uint32_t now);
bool Leds_NoticeComplete(uint32_t now);
#endif
