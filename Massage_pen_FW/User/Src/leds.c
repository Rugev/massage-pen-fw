#include "leds.h"
#include "main.h"
#include <stddef.h>
static Leds_Display current;
static uint32_t phase_start;
static GPIO_TypeDef *const heat_ports[]={LED_HEAT_1_GPIO_Port,LED_HEAT_2_GPIO_Port,LED_HEAT_3_GPIO_Port};
static const uint16_t heat_pins[]={LED_HEAT_1_Pin,LED_HEAT_2_Pin,LED_HEAT_3_Pin};
static GPIO_TypeDef *const vibration_ports[]={LED_VIBRATION_1_GPIO_Port,LED_VIBRATION_2_GPIO_Port,LED_VIBRATION_3_GPIO_Port};
static const uint16_t vibration_pins[]={LED_VIBRATION_1_Pin,LED_VIBRATION_2_Pin,LED_VIBRATION_3_Pin};
static void write(GPIO_TypeDef *port,uint16_t pin,bool on) {
 HAL_GPIO_WritePin(port,pin,(on==(LEDS_ACTIVE_HIGH!=0U)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
void Leds_Init(uint32_t now) { current=(Leds_Display){.mode=LEDS_OFF}; phase_start=now; Leds_Update(now); }
void Leds_Request(const Leds_Display *d,uint32_t now) {
 if(d==NULL) return;
 if(current.mode!=d->mode || current.heat_level!=d->heat_level || current.vibration_level!=d->vibration_level ||
 current.heater_breathe!=d->heater_breathe || current.battery_red!=d->battery_red || current.battery_green!=d->battery_green || current.fault_code!=d->fault_code) phase_start=now;
 current=*d;
}
bool Leds_NoticeComplete(uint32_t now) {
 return current.mode==LEDS_LOW_BATTERY && (uint32_t)(now-phase_start)>=LEDS_NOTICE_FLASHES*2U*LEDS_NOTICE_HALF_PERIOD_MS;
}
void Leds_Update(uint32_t now) {
 uint32_t elapsed=now-phase_start; bool red=false,green=false,heat_on=true;
 uint8_t heat=0U,vibration=0U;
 if(current.mode==LEDS_LEVELS) {
  heat=current.heat_level>LEDS_MAX_LEVEL ? LEDS_MAX_LEVEL : current.heat_level;
  vibration=current.vibration_level>LEDS_MAX_LEVEL ? LEDS_MAX_LEVEL : current.vibration_level;
  red=current.battery_red; green=current.battery_green;
  if(current.heater_breathe) {
   uint32_t phase=elapsed%LEDS_BREATHE_PERIOD_MS;
   uint32_t half=LEDS_BREATHE_PERIOD_MS/2U;
   uint32_t triangle=phase<half ? phase : LEDS_BREATHE_PERIOD_MS-phase;
   uint32_t duty=(triangle*LEDS_MODULATION_STEPS)/half;
   /* One 1 ms pulse-density step. Coprime permutation avoids a 32 ms
    * contiguous on/off burst while retaining a bounded calculation. */
   uint32_t slot=(phase*LEDS_MODULATION_STRIDE)%LEDS_MODULATION_STEPS;
   heat_on=slot<duty;
  }
 } else if(current.mode==LEDS_LOW_BATTERY) {
  red=!Leds_NoticeComplete(now) && ((elapsed/LEDS_NOTICE_HALF_PERIOD_MS)%2U==0U);
 } else if(current.mode==LEDS_FAULT) {
  red=(elapsed/LEDS_FAULT_HALF_PERIOD_MS)%2U==0U;
 }
 for(unsigned i=0;i<LEDS_MAX_LEVEL;i++) {
  bool h=current.mode==LEDS_FAULT ? (current.fault_code & (1U<<i))!=0U : (i<heat && heat_on);
  bool v=current.mode==LEDS_FAULT ? (current.fault_code & (1U<<(i+LEDS_MAX_LEVEL)))!=0U : i<vibration;
  write(heat_ports[i],heat_pins[i],h); write(vibration_ports[i],vibration_pins[i],v);
 }
 write(BAT_LED_R_GPIO_Port,BAT_LED_R_Pin,red); write(BAT_LED_G_GPIO_Port,BAT_LED_G_Pin,green);
}
