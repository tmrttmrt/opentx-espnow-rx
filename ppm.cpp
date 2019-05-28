#include <Arduino.h>
#include "esprx.h"

int16_t locChannelOutputs[MAX_OUTPUT_CHANNELS];
static volatile uint32_t pulses[20];
static volatile uint32_t *ptr;
static volatile int outLevel;
const unsigned int mask = 1;


#define GPIO_PPM_PIN  12
#define US_TICKS  5
#define POLARITY 1

void setupPulsesPPM(int channelsStart, int frameLength)
{
  const uint32_t pw = 300 * US_TICKS;
  const uint32_t PPM_min = 1000 * US_TICKS;
  const uint32_t PPM_max = 2000 * US_TICKS;  
  const uint32_t PPM_offs = 1500 * US_TICKS;
  const uint32_t minRest = 4500 * US_TICKS;
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
  outLevel = 1;
}

void timer_callback()
{
  noInterrupts();
  digitalWrite(GPIO_PPM_PIN, outLevel);
  outLevel++;
  outLevel = outLevel & mask;
  if ( 0 == *ptr ){
    setupPulsesPPM(0, 22);
    outLevel = POLARITY;
  } 
  timer1_write(*ptr++);
  interrupts();
}

void initPPM() {

  pinMode(GPIO_PPM_PIN, OUTPUT);
  
  timer1_isr_init();
  timer1_attachInterrupt(timer_callback);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
  setupPulsesPPM(0, 22);
  timer_callback();
}

int16_t get_ch(uint16_t idx){
  return locChannelOutputs[idx];
}
