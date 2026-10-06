#ifndef USER_BUTTONS_H
#define USER_BUTTONS_H
#include <stdbool.h>
#include <stdint.h>
#define BUTTONS_ACTIVE_HIGH 1U
#define BUTTONS_DEBOUNCE_MS 20U
#define BUTTONS_EVENT_PRESSED 1U
#define BUTTONS_EVENT_RELEASED 2U
typedef enum { BUTTONS_POWER, BUTTONS_VIBRATION, BUTTONS_HEAT, BUTTONS_COUNT } Buttons_Id;
typedef struct { uint8_t flags; uint32_t press_id; uint32_t duration_ms; } Buttons_Event;
void Buttons_Init(uint32_t now);
void Buttons_Update(uint32_t now);
/* Consume at least once each foreground tick. Flags retain both edges until consumed. */
bool Buttons_TakeEvent(Buttons_Id id, Buttons_Event *event);
bool Buttons_IsPressed(Buttons_Id id);
/* Accepted debounced edge timestamps; returns retained final duration after release. */
uint32_t Buttons_PressDuration(Buttons_Id id, uint32_t now);
uint32_t Buttons_PressIdentity(Buttons_Id id);
/* Adopt authoritative waking-press evidence with its accepted debounced timestamp.
 * A new timestamp replaces stale cached state; repeating the same timestamp is
 * idempotent, including after release. Retain the identity through validation. */
void Buttons_AdoptWakePress(Buttons_Id id, uint32_t started_at);
/* Once per raw power wake: discard the stale episode and start stable-input
 * debounce at now. No press identity or accepted timestamp exists until stable. */
void Buttons_RearmWakePress(Buttons_Id id, uint32_t now);
#endif
