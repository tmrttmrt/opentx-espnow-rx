#include <WifiEspNow.h>
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif
#include "esprx.h"


int16_t get_ch(uint16_t idx);
extern uint32_t volatile recvTime;

void setup() {
  Serial.begin(115200);
  Serial.println();

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESPNOW", nullptr, 1);
  WiFi.softAPdisconnect(false);

  Serial.print("MAC address of this node is ");
  Serial.println(WiFi.softAPmacAddress());
  initRX();
}

void loop()
{
  char buff[8*10];
  delay(1000);
  *buff = 0;
  char *p = buff;
  for(int i = 0; i < 8; i++){
    sprintf(p,"%2d:%4d, ", i, get_ch(i));
    p = buff + strlen(buff);
  }
  Serial.println( "Data age (ms): " + String(millis()- recvTime)+ " received: " + String(packRecv));
  Serial.println(buff);
}
