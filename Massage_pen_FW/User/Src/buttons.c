#include "buttons.h"
#include "main.h"
#include <string.h>
typedef struct {
 bool candidate, pressed;
 uint32_t candidate_since, started_at, identity;
 Buttons_Event event;
} Button;
static Button buttons[BUTTONS_COUNT];
static uint32_t next_identity;
static GPIO_TypeDef *const ports[BUTTONS_COUNT]={BUTTON_PWR_ON_GPIO_Port,BUTTON_VIBRATION_GPIO_Port,BUTTON_HEAT_GPIO_Port};
static const uint16_t pins[BUTTONS_COUNT]={BUTTON_PWR_ON_Pin,BUTTON_VIBRATION_Pin,BUTTON_HEAT_Pin};
static bool raw(Buttons_Id id) { return (HAL_GPIO_ReadPin(ports[id],pins[id])==GPIO_PIN_SET)==(BUTTONS_ACTIVE_HIGH!=0U); }
static void press(Button *b, uint32_t now)
{
 b->pressed = true;
 b->started_at = now;
 if (++next_identity == 0U) ++next_identity;
 b->identity = next_identity;
 b->event.flags |= BUTTONS_EVENT_PRESSED;
 b->event.press_id = b->identity;
 b->event.duration_ms = 0U;
}
void Buttons_Init(uint32_t now) {
 memset(buttons,0,sizeof buttons);
 for(unsigned i=0;i<BUTTONS_COUNT;i++) { buttons[i].candidate=raw((Buttons_Id)i); buttons[i].candidate_since=now; }
}
void Buttons_Update(uint32_t now)
{
 for (unsigned i = 0; i < BUTTONS_COUNT; i++) {
  Button *b = &buttons[i];
  bool value = raw((Buttons_Id)i);
  if (value != b->candidate) {
   b->candidate = value;
   b->candidate_since = now;
  }
  if (b->candidate != b->pressed &&
      (uint32_t)(now - b->candidate_since) >= BUTTONS_DEBOUNCE_MS) {
   if (b->candidate) press(b, now);
   else {
    b->pressed = false;
    b->event.flags |= BUTTONS_EVENT_RELEASED;
    b->event.press_id = b->identity;
    b->event.duration_ms = now - b->started_at;
   }
  }
 }
}
bool Buttons_TakeEvent(Buttons_Id id,Buttons_Event *event) {
 if((unsigned)id>=BUTTONS_COUNT || event==NULL || buttons[id].event.flags==0U) return false;
 *event=buttons[id].event; buttons[id].event.flags=0U; return true;
}
bool Buttons_IsPressed(Buttons_Id id) { return (unsigned)id<BUTTONS_COUNT && buttons[id].pressed; }
uint32_t Buttons_PressDuration(Buttons_Id id,uint32_t now) {
 if((unsigned)id>=BUTTONS_COUNT) return 0U;
 return buttons[id].pressed ? now-buttons[id].started_at : buttons[id].event.duration_ms;
}
uint32_t Buttons_PressIdentity(Buttons_Id id) { return (unsigned)id<BUTTONS_COUNT ? buttons[id].identity : 0U; }
void Buttons_AdoptWakePress(Buttons_Id id, uint32_t started_at)
{
 if ((unsigned)id >= BUTTONS_COUNT) return;
 Button *b = &buttons[id];
 /* A verified new episode supersedes stale held state retained through sleep.
  * Repeated evidence for the same episode must preserve identity/consumption. */
 if (b->identity != 0U && b->started_at == started_at) return;
 b->candidate = true;
 b->candidate_since = started_at;
 b->event = (Buttons_Event){0};
 press(b, started_at);
}
void Buttons_RearmWakePress(Buttons_Id id, uint32_t now)
{
 if ((unsigned)id >= BUTTONS_COUNT) return;
 buttons[id] = (Button){.candidate = raw(id), .candidate_since = now};
}
