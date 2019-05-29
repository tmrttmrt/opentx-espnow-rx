#include <Arduino.h>
#include "esprx.h"

#define GPIO_PPM_PIN  12
#define US_TICKS  5
#define POLARITY 1

#if defined(ESP8266)
#define IRAM_ATTR
#elif defined(ESP32)
static hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
#endif

int16_t locChannelOutputs[MAX_OUTPUT_CHANNELS];
static volatile uint32_t pulses[20];
static volatile uint32_t *ptr;
static volatile int outLevel;
static unsigned int mask = 1;
static int frameLength = 22; //22 ms 
static volatile bool ppmEnabled = true;


IRAM_ATTR void setupPulsesPPM(int channelsStart)
{
  static uint32_t pw = 300 * US_TICKS;
  static uint32_t PPM_min = 1000 * US_TICKS;
  static uint32_t PPM_max = 2000 * US_TICKS;  
  static uint32_t PPM_offs = 1500 * US_TICKS;
  static uint32_t minRest = 4500 * US_TICKS;

  unsigned int firstCh = channelsStart;
  unsigned int lastCh = firstCh + 8;
  lastCh = lastCh > MAX_OUTPUT_CHANNELS ? MAX_OUTPUT_CHANNELS : lastCh;

  ptr = pulses;
  uint32_t rest = frameLength * 1000 * US_TICKS;
  for (int i=firstCh; i<lastCh; i++) {
    uint32_t v = (locChannelOutputs[i] * US_TICKS)/2 + PPM_offs;
    v = v < PPM_min ? PPM_min : v;
    v = v > PPM_max ? PPM_max : v;
    rest -= v;
    *ptr++ = pw;
    *ptr++ = v-pw;
  }
  *ptr++ = pw;
  if (rest < minRest) rest = minRest;
  *ptr++ = rest;
  *ptr = 0;
  ptr = pulses;
  outLevel = POLARITY;
}


IRAM_ATTR void timer_callback()
{
  static bool intEn = true;
#if defined(ESP8266)
  noInterrupts();
#elif defined(ESP32)
  portENTER_CRITICAL_ISR(&timerMux);
#endif
  digitalWrite(GPIO_PPM_PIN, outLevel);
  outLevel++;
  outLevel = outLevel & mask;
  if ( 0 == *ptr) {
    if (ppmEnabled){
      setupPulsesPPM(0);
    }
    intEn = ppmEnabled;
  }
#if defined(ESP8266)
  timer1_write(*ptr++);
  interrupts();
#elif defined(ESP32)
  if (intEn) {
    timerAlarmWrite(timer, *ptr++, true);
  } 
  else {
    timerStop(timer);
  }
  portEXIT_CRITICAL_ISR(&timerMux);
#endif
}

void initPPM() {
  pinMode(GPIO_PPM_PIN, OUTPUT);
#if defined(ESP8266)
  timer1_isr_init();
  timer1_attachInterrupt(timer_callback);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
#elif defined(ESP32)
  timer = timerBegin(0, 16, true); // 5 MHz
  timerAttachInterrupt(timer, timer_callback, true);
#endif
  setupPulsesPPM(0);
  timer_callback();
#if defined(ESP32)  
  timerAlarmEnable(timer);
#endif  
}

void disablePPM(){
  ppmEnabled = false;
  delay(2*frameLength);
}

void enablePPM(){
  ppmEnabled = true;
#if defined(ESP32)  
  timerRestart(timer);
#endif  
}

int16_t get_ch(uint16_t idx){
  return locChannelOutputs[idx];
}
