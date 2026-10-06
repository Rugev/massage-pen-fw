#include <assert.h>
#include <stdio.h>
#include "fake_hal.h"
#include "main.h"
#include "buttons.h"
#include "leds.h"
#include "storage.h"
#define OUT(name) FakeHAL_GetOutput(name##_GPIO_Port,name##_Pin)
static void buttons(void) {
 Buttons_Event e;
 FakeHAL_Reset(); Buttons_Init(0);
 FakeHAL_SetInput(BUTTON_HEAT_GPIO_Port,BUTTON_HEAT_Pin,GPIO_PIN_SET);
 Buttons_Update(1); Buttons_Update(20); assert(!Buttons_TakeEvent(BUTTONS_HEAT,&e));
 FakeHAL_SetInput(BUTTON_HEAT_GPIO_Port,BUTTON_HEAT_Pin,GPIO_PIN_RESET); Buttons_Update(21);
 FakeHAL_SetInput(BUTTON_HEAT_GPIO_Port,BUTTON_HEAT_Pin,GPIO_PIN_SET); Buttons_Update(22);
 Buttons_Update(41); assert(!Buttons_IsPressed(BUTTONS_HEAT)); Buttons_Update(42);
 assert(Buttons_TakeEvent(BUTTONS_HEAT,&e)); assert(e.flags==BUTTONS_EVENT_PRESSED);
 uint32_t identity=e.press_id; assert(identity!=0); assert(Buttons_PressDuration(BUTTONS_HEAT,142)==100);
 FakeHAL_SetInput(BUTTON_HEAT_GPIO_Port,BUTTON_HEAT_Pin,GPIO_PIN_RESET); Buttons_Update(142);
 Buttons_Update(161); assert(Buttons_IsPressed(BUTTONS_HEAT)); Buttons_Update(162);
 assert(Buttons_TakeEvent(BUTTONS_HEAT,&e)); assert(e.flags==BUTTONS_EVENT_RELEASED && e.duration_ms==120 && e.press_id==identity);
 assert(!Buttons_TakeEvent(BUTTONS_HEAT,&e));
 Buttons_Init(UINT32_MAX-30U); FakeHAL_SetInput(BUTTON_PWR_ON_GPIO_Port,BUTTON_PWR_ON_Pin,GPIO_PIN_SET);
 Buttons_Update(UINT32_MAX-25U); Buttons_Update(UINT32_MAX-5U);
 assert(Buttons_IsPressed(BUTTONS_POWER)); assert(Buttons_PressDuration(BUTTONS_POWER,14)==20);
 FakeHAL_SetInput(BUTTON_PWR_ON_GPIO_Port,BUTTON_PWR_ON_Pin,GPIO_PIN_RESET); Buttons_Update(14); Buttons_Update(34);
 assert(Buttons_TakeEvent(BUTTONS_POWER,&e)); assert(e.flags==3 && e.duration_ms==40);
 Buttons_Init(100); FakeHAL_SetInput(BUTTON_PWR_ON_GPIO_Port,BUTTON_PWR_ON_Pin,GPIO_PIN_SET);
 Buttons_AdoptWakePress(BUTTONS_POWER,80); identity=Buttons_PressIdentity(BUTTONS_POWER);
 Buttons_Update(300); assert(Buttons_PressIdentity(BUTTONS_POWER)==identity); assert(Buttons_PressDuration(BUTTONS_POWER,380)==300);
 FakeHAL_SetInput(BUTTON_PWR_ON_GPIO_Port,BUTTON_PWR_ON_Pin,GPIO_PIN_RESET); Buttons_Update(390); Buttons_Update(410);
 assert(Buttons_TakeEvent(BUTTONS_POWER,&e)); assert(e.press_id==identity && e.duration_ms==330 && e.flags==3);
 Buttons_Init(UINT32_MAX-50U); FakeHAL_SetInput(BUTTON_PWR_ON_GPIO_Port,BUTTON_PWR_ON_Pin,GPIO_PIN_SET);
 Buttons_AdoptWakePress(BUTTONS_POWER,UINT32_MAX-20U); identity=Buttons_PressIdentity(BUTTONS_POWER);
 Buttons_AdoptWakePress(BUTTONS_POWER,UINT32_MAX-20U); assert(Buttons_PressIdentity(BUTTONS_POWER)==identity);
 Buttons_Update(9); assert(Buttons_PressDuration(BUTTONS_POWER,9)==30);
 FakeHAL_SetInput(BUTTON_PWR_ON_GPIO_Port,BUTTON_PWR_ON_Pin,GPIO_PIN_RESET); Buttons_Update(9); Buttons_Update(29);
 assert(Buttons_TakeEvent(BUTTONS_POWER,&e)); assert(e.press_id==identity && e.duration_ms==50);
 FakeHAL_Reset(); Buttons_Init(0);
 FakeHAL_SetInput(BUTTON_VIBRATION_GPIO_Port,BUTTON_VIBRATION_Pin,GPIO_PIN_SET); Buttons_Update(1); Buttons_Update(21);
 assert(Buttons_TakeEvent(BUTTONS_VIBRATION,&e) && e.flags==BUTTONS_EVENT_PRESSED);
}
/* A verified sleeping release/repress must replace the cached held episode. */
static void verified_wake_replaces_cached_press(void)
{
 for (unsigned rollover = 0U; rollover < 2U; ++rollover) {
  uint32_t base = rollover ? UINT32_MAX - 200U : 0U;
  FakeHAL_Reset();
  Buttons_Init(base);
  FakeHAL_SetInput(BUTTON_PWR_ON_GPIO_Port, BUTTON_PWR_ON_Pin, GPIO_PIN_SET);
  Buttons_Update(base);
  Buttons_Update(base + 20U);
  uint32_t old_identity = Buttons_PressIdentity(BUTTONS_POWER);
  Buttons_Event event;
  assert(Buttons_TakeEvent(BUTTONS_POWER, &event));

  /* No foreground sampling observes the sleeping release. */
  FakeHAL_SetInput(BUTTON_PWR_ON_GPIO_Port, BUTTON_PWR_ON_Pin, GPIO_PIN_RESET);
  FakeHAL_SetInput(BUTTON_PWR_ON_GPIO_Port, BUTTON_PWR_ON_Pin, GPIO_PIN_SET);
  uint32_t wake_started = base + 300U;
  Buttons_AdoptWakePress(BUTTONS_POWER, wake_started);
  uint32_t wake_identity = Buttons_PressIdentity(BUTTONS_POWER);
  assert(wake_identity != 0U && wake_identity != old_identity);
  assert(Buttons_PressDuration(BUTTONS_POWER, wake_started + 299U) == 299U);
  assert(Buttons_TakeEvent(BUTTONS_POWER, &event));
  assert(event.flags == BUTTONS_EVENT_PRESSED && event.press_id == wake_identity);
  Buttons_AdoptWakePress(BUTTONS_POWER, wake_started);
  assert(Buttons_PressIdentity(BUTTONS_POWER) == wake_identity);
  assert(!Buttons_TakeEvent(BUTTONS_POWER, &event));
  Buttons_Update(wake_started + 300U);
  assert(Buttons_PressDuration(BUTTONS_POWER, wake_started + 300U) == 300U);

  FakeHAL_SetInput(BUTTON_PWR_ON_GPIO_Port, BUTTON_PWR_ON_Pin, GPIO_PIN_RESET);
  Buttons_Update(wake_started + 310U);
  Buttons_Update(wake_started + 330U);
  assert(Buttons_TakeEvent(BUTTONS_POWER, &event));
  assert(event.flags == BUTTONS_EVENT_RELEASED && event.duration_ms == 330U);
  Buttons_AdoptWakePress(BUTTONS_POWER, wake_started);
  assert(!Buttons_IsPressed(BUTTONS_POWER));
  assert(Buttons_PressIdentity(BUTTONS_POWER) == wake_identity);
  assert(!Buttons_TakeEvent(BUTTONS_POWER, &event));
 }
}
static void raw_wake_rearms_only_its_button_and_requires_stability(void)
{
 FakeHAL_Reset();
 uint32_t wake = UINT32_MAX - 10U;
 Buttons_Init(wake - 100U);
 FakeHAL_SetInput(BUTTON_PWR_ON_GPIO_Port, BUTTON_PWR_ON_Pin, GPIO_PIN_SET);
 FakeHAL_SetInput(BUTTON_HEAT_GPIO_Port, BUTTON_HEAT_Pin, GPIO_PIN_SET);
 Buttons_Update(wake - 100U);
 Buttons_Update(wake - 80U);
 uint32_t old_identity = Buttons_PressIdentity(BUTTONS_POWER);
 uint32_t heat_identity = Buttons_PressIdentity(BUTTONS_HEAT);
 Buttons_RearmWakePress(BUTTONS_POWER, wake);
 Buttons_Event event;
 assert(!Buttons_IsPressed(BUTTONS_POWER));
 assert(!Buttons_TakeEvent(BUTTONS_POWER, &event));
 assert(Buttons_PressDuration(BUTTONS_POWER, wake) == 0U);
 assert(Buttons_IsPressed(BUTTONS_HEAT));
 assert(Buttons_PressIdentity(BUTTONS_HEAT) == heat_identity);
 Buttons_Update(wake + 19U);
 assert(!Buttons_IsPressed(BUTTONS_POWER));
 /* A bounce restarts the full stable-input interval. */
 FakeHAL_SetInput(BUTTON_PWR_ON_GPIO_Port, BUTTON_PWR_ON_Pin, GPIO_PIN_RESET);
 Buttons_Update(wake + 19U);
 FakeHAL_SetInput(BUTTON_PWR_ON_GPIO_Port, BUTTON_PWR_ON_Pin, GPIO_PIN_SET);
 Buttons_Update(wake + 20U);
 Buttons_Update(wake + 39U);
 assert(!Buttons_IsPressed(BUTTONS_POWER));
 Buttons_Update(wake + 40U);
 assert(Buttons_TakeEvent(BUTTONS_POWER, &event));
 assert(event.flags == BUTTONS_EVENT_PRESSED);
 assert(event.press_id != 0U && event.press_id != old_identity);
 assert(Buttons_PressDuration(BUTTONS_POWER, wake + 40U) == 0U);
 assert(Buttons_PressDuration(BUTTONS_POWER, wake + 340U) == 300U);
}
static void leds(void) {
 FakeHAL_Reset(); Leds_Init(0);
 Leds_Display d={.mode=LEDS_LEVELS,.heat_level=2,.vibration_level=1,.battery_green=true};
 Leds_Request(&d,0); Leds_Update(0);
 assert(OUT(LED_HEAT_1) && OUT(LED_HEAT_2) && !OUT(LED_HEAT_3));
 assert(OUT(LED_VIBRATION_1) && !OUT(LED_VIBRATION_2) && !OUT(LED_VIBRATION_3)); assert(OUT(BAT_LED_G));
 for(unsigned level=0;level<=3;level++) {
 d.heat_level=(uint8_t)level; d.vibration_level=(uint8_t)level; Leds_Request(&d,0); Leds_Update(0);
 assert(OUT(LED_HEAT_1)==(level>=1) && OUT(LED_HEAT_2)==(level>=2) && OUT(LED_HEAT_3)==(level>=3));
 assert(OUT(LED_VIBRATION_1)==(level>=1) && OUT(LED_VIBRATION_2)==(level>=2) && OUT(LED_VIBRATION_3)==(level>=3));
 }
 d.heat_level=2; d.vibration_level=1; d.heater_breathe=true; Leds_Request(&d,0);
 unsigned counts[4]={0}; unsigned waveform[2000];
 for(uint32_t t=0;t<2000;t++) { Leds_Request(&d,t); Leds_Update(t); waveform[t]=OUT(LED_HEAT_1); counts[t/500]+=OUT(LED_HEAT_1); assert(OUT(LED_HEAT_1)==OUT(LED_HEAT_2)); assert(!OUT(LED_HEAT_3)); assert(OUT(LED_VIBRATION_1)); }
 assert(counts[0]<counts[1] && counts[3]<counts[2]);
 for(uint32_t t=0;t<2000;t++) { Leds_Update(t+2000U); assert(OUT(LED_HEAT_1)==waveform[t]); }
 d=(Leds_Display){.mode=LEDS_LOW_BATTERY}; uint32_t start=UINT32_MAX-100U; Leds_Request(&d,start);
 for(uint32_t t=0;t<3000;t++) { Leds_Request(&d,start+t); Leds_Update(start+t); assert(OUT(BAT_LED_R)==((t/500)%2==0)); assert(!Leds_NoticeComplete(start+t)); }
 Leds_Update(start+3000U); assert(!OUT(BAT_LED_R) && Leds_NoticeComplete(start+3000U));
 const uint16_t pins[]={LED_HEAT_1_Pin,LED_HEAT_2_Pin,LED_HEAT_3_Pin,LED_VIBRATION_1_Pin,LED_VIBRATION_2_Pin,LED_VIBRATION_3_Pin};
 GPIO_TypeDef *ports[]={LED_HEAT_1_GPIO_Port,LED_HEAT_2_GPIO_Port,LED_HEAT_3_GPIO_Port,LED_VIBRATION_1_GPIO_Port,LED_VIBRATION_2_GPIO_Port,LED_VIBRATION_3_GPIO_Port};
 for(unsigned bit=0;bit<6;bit++) { d=(Leds_Display){.mode=LEDS_FAULT,.fault_code=(uint8_t)(1U<<bit)}; Leds_Request(&d,4000);
 for(uint32_t t=0;t<400;t++) { Leds_Request(&d,4000+t); Leds_Update(4000+t); assert(OUT(BAT_LED_R)==((t/100)%2==0)); for(unsigned j=0;j<6;j++) assert(FakeHAL_GetOutput(ports[j],pins[j])==(j==bit)); } }
 d=(Leds_Display){.mode=LEDS_OFF}; Leds_Request(&d,5000); Leds_Update(5000); assert(!OUT(BAT_LED_R) && !OUT(BAT_LED_G));
 for(unsigned j=0;j<6;j++) assert(!FakeHAL_GetOutput(ports[j],pins[j]));
}
int main(void) { raw_wake_rearms_only_its_button_and_requires_stability(); verified_wake_replaces_cached_press(); buttons(); leds(); Storage_Settings s=Storage_Load(); assert(s.heat_level==1 && s.vibration_level==1); s.heat_level=3; s.vibration_level=0; s=Storage_Load(); assert(s.heat_level==1 && s.vibration_level==1); puts("ui tests passed"); }
