#include "esprx.h"


int16_t get_ch(uint16_t idx);
extern uint32_t volatile recvTime;

void setup() {
  Serial.begin(115200);
  Serial.println();
  initRX();
  initPPM();
}

void loop()
{
  static char buff[8*10];
  delay(1000);
  checkEEPROM();
  *buff = 0;
  char *p = buff;
  for(int i = 0; i < 8; i++){
    sprintf(p,"%2d:%4d, ", i, get_ch(i));
    p = buff + strlen(buff);
  }
  Serial.println( "Data age (ms): " + String(millis()- recvTime)+ " received: " + String(packRecv));
  Serial.println(buff);
  
}
