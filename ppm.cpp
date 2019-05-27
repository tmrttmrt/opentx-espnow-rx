#include <Arduino.h>
#include "esprx.h"

int16_t locChannelOutputs[MAX_OUTPUT_CHANNELS];
static volatile uint16_t pulses[20];
static volatile uint16_t *ptr;
static volatile int outLevel;
const unsigned int mask = 1;


#define GPIO_PPM_PIN 12
#define GPIO_OUTPUT_PIN_SEL  (1ULL << GPIO_PPM_PIN)

void setupPulsesPPM(int channelsStart, int frameLength)
{
  const int PPM_min = 1000;
  const int PPM_max = 2000;  
  const int PPM_offs = 1500;
  const int pw = 300;
  const int minRest = 4500;
  unsigned int firstCh = channelsStart;
  unsigned int lastCh = firstCh + 8;
  lastCh = lastCh > MAX_OUTPUT_CHANNELS ? MAX_OUTPUT_CHANNELS : lastCh;

  ptr = pulses;
  int rest = frameLength * 1000;
  for (int i=firstCh; i<lastCh; i++) {
    int v = locChannelOutputs[i]/2 + PPM_offs;
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
  outLevel = 1;
}

void hw_timer_callback(void *arg)
{
  digitalWrite(GPIO_PPM_PIN, outLevel);
  outLevel++;
  outLevel = outLevel & mask;
  if ( 0 == *ptr ){
    setupPulsesPPM(0, 22);
    outLevel = 1;
  } 
//  hw_timer_alarm_us(*ptr++, false);
}

void initPPM() {

  pinMode(GPIO_PPM_PIN, OUTPUT);
  
  setupPulsesPPM(0, 22);
  hw_timer_callback(NULL);
}

int16_t get_ch(uint16_t idx){
  return locChannelOutputs[idx];
}
