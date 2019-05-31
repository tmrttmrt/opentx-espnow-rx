#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <Arduino.h>
#include <EEPROM.h>
#if defined(ESP8266)
#include <ESP8266WiFi.h>
//Binding (broadcast) from esp32 seems to work only with non-os-sdk 2.2.2
#elif defined(ESP32)
#include <WiFi.h>
#include <esp_wifi.h>
#define LED_BUILTIN 2
#endif
#include <WifiEspNow.h>
#include "esprx.h"
#include "uCRC16Lib.h"
#include <Ticker.h>

#if defined(ESP8266)
#define ADD_PEER(mac, chan, mode) WifiEspNow.addPeer(mac, chan)
#elif defined(ESP32)
#define ADD_PEER(mac, chan, mode)  WifiEspNow.addPeer(mac, chan, NULL, (int)mode)
#endif


static struct WifiEspNowPeerInfo txPeer = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},1};
static RXPacket_t packet;
static volatile bool dirty = false;
static volatile bool bindEnabled = true;
static uint32_t startTime;
uint32_t volatile packRecv = 0;
uint32_t volatile packAckn = 0;
uint32_t volatile recvTime = 0;
Ticker blinker;

char *mac2str(const uint8_t mac[6]) {
  static char buff[18];
  sprintf(buff, "%02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return buff;
}

static bool packet_check(TXPacket_t * rxp)
{
  packet.idx++;
  uint16_t crc = rxp->crc;
  rxp->crc = 0;
  packet.crc = uCRC16Lib::calculate((char *) rxp, sizeof(TXPacket_t));
  if ( packet.crc == crc) {
    packet.idx = rxp->idx;
    packet.type = ACK;
    return true;
  }
  Serial.printf("CRC16 error: %d vs %d\n", packet.crc, crc);
  return false;
}

static inline void process_data(const uint8_t mac[6], TXPacket_t *rp){
  if (packet_check(rp)) {
    recvTime = millis();
#if defined(ESP32)
    portENTER_CRITICAL(&timerMux);
#endif
    memcpy( (void*) locChannelOutputs, rp->ch, sizeof(locChannelOutputs) );
#if defined(ESP32)
    portEXIT_CRITICAL(&timerMux);
#endif
    packRecv++;
    WifiEspNow.send(txPeer.mac, (const uint8_t *) &packet, sizeof(packet));
    if (FSAFE == rp->type) {
      dirty = true;
    }
  }
  else {
    Serial.printf("Packet error from MAC: %s\n", mac2str(mac));
  }

}

static inline void process_bind(const uint8_t mac[6], TXPacket_t *rp) {
  if (BIND == rp->type) {
    uint16_t crc = rp->crc;
    rp->crc = 0;
    rp->crc = uCRC16Lib::calculate((char *) rp, sizeof(TXPacket_t));
    if(crc == rp->crc){
      Serial.printf("Got bind MAC: %s", mac2str(mac));
      memcpy(txPeer.mac, mac, sizeof(txPeer.mac));
      txPeer.channel = rp->ch[0];
      if (!ADD_PEER(txPeer.mac, txPeer.channel, ESP_IF_WIFI_STA)) {
        Serial.printf("ADD_PEER() failed MAC: %s", mac2str(txPeer.mac));
      }
      packet.type = BIND;
      packet.crc=0;
      packet.crc = uCRC16Lib::calculate((char *) &packet, sizeof(packet));
      WifiEspNow.send(txPeer.mac, (const uint8_t *) &packet, sizeof(packet));
      dirty = true;
      bindEnabled = false;
    }
    else {
      Serial.printf("Bind packet CRC error: %d vs %d", crc, rp->crc);
    }
  }
  else {
    Serial.printf("Incorrect packet type: %d", rp->type);
  }
}

void recv_cb(const uint8_t mac[6], const uint8_t* buf, size_t count, void* cbarg) {
  if (sizeof(TXPacket_t) == count) {
    TXPacket_t *rp = (TXPacket_t *) buf;
    if (bindEnabled && BIND == rp->type) {
      process_bind(mac, rp);
    } 
    else if (DATA == rp->type || FSAFE == rp->type) {
      if (!memcmp(mac, txPeer.mac, sizeof(txPeer.mac))) {
        process_data(mac, rp);
      }
      else {
        Serial.printf("Wrong MAC: %s, %s\n", String(mac2str(mac)).c_str(),   String(mac2str(txPeer.mac)).c_str() );
      }
    }
  }
  else {
    Serial.printf("Wrong packet size: %d (must be %d)\n",  count, sizeof(TXPacket_t));
  }
}

void blink() {
  static bool fast = true;
  int state = digitalRead( GPIO_LED_PIN);
  digitalWrite( GPIO_LED_PIN, !state);
  uint32_t dataAge = millis()-recvTime;
  if (dataAge > 100){
    if (fast) {
      blinker.attach(1, blink);
      fast = false;
    }
  }
  else {
    if (!fast) {
      blinker.attach(0.1, blink);
      fast = true;
    }
  }
  if (dataAge > FS_TIMEOUT) {
    memcpy(locChannelOutputs, fsChannelOutputs, sizeof(locChannelOutputs));
  }
}

void checkEEPROM() {
  static bool blinking = false;
  if (dirty) {
    disablePPM();
    EEPROM.put(0,txPeer);
    EEPROM.put(sizeof(txPeer), fsChannelOutputs);
    EEPROM.commit();
    dirty = false;
    enablePPM();
  }
  if (bindEnabled) {
    if(millis()-startTime > BIND_TIMEOUT) {
      bindEnabled = false;
    }
  }
  else if(!blinking) {
#if defined(ESP8266)
    wifi_set_channel(txPeer.channel);
#elif defined(ESP32)  
    esp_wifi_set_channel( txPeer.channel, (wifi_second_chan_t) 0);
#endif
    blinker.attach(0.1, blink);
    blinking = true;
  }
}
 
void initRX(){

  startTime = millis();
  pinMode(GPIO_LED_PIN, OUTPUT);
  digitalWrite(GPIO_LED_PIN,0);
  
  EEPROM.begin(sizeof(txPeer)+sizeof(fsChannelOutputs));
  EEPROM.get(0,txPeer);
  EEPROM.get(sizeof(txPeer), fsChannelOutputs);

#if defined(ESP8266)
    WiFi.mode(WIFI_STA);
    wifi_set_channel(BIND_CH);  
#elif defined(ESP32)  
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(BIND_CH, (wifi_second_chan_t) 0);
#endif

  if (!WifiEspNow.begin()) {
    Serial.println("WifiEspNow.begin() failed");
    return;
  }

  WifiEspNow.onReceive(recv_cb, nullptr);

  if (!ADD_PEER(txPeer.mac, txPeer.channel, ESP_IF_WIFI_STA)) {
    Serial.printf("ADD_PEER() failed MAC: %s", mac2str(txPeer.mac));

  }
  if (!ADD_PEER(broadcast_mac, BIND_CH, ESP_IF_WIFI_STA)) {
    Serial.printf("ADD_PEER() failed: MAC: %s", mac2str(broadcast_mac));

  }
}
