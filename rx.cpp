#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <Arduino.h>
#include <EEPROM.h>
#include <WifiEspNow.h>
#include "esprx.h"
#include "uCRC16Lib.h"

#define BIND_TIMEOUT 10000

static struct WifiEspNowPeerInfo txPeer = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},1};
static RXPacket_t packet;
static volatile bool dirty = false;
static volatile bool bindEnabled = true;
static uint32_t startTime;
uint32_t volatile packRecv = 0;
uint32_t volatile packAckn = 0;
uint32_t volatile recvTime = 0;

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
      if (!WifiEspNow.hasPeer(txPeer.mac)) {
        WifiEspNow.addPeer(txPeer.mac, txPeer.channel);
      }
      packet.type = BIND;
      packet.crc=0;
      packet.crc = uCRC16Lib::calculate((char *) &packet, sizeof(packet));
      WifiEspNow.send(txPeer.mac, (const uint8_t *) &packet, sizeof(packet));
      dirty = true;
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
    else if (DATA == rp->type) {
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

void checkEEPROM() {
  if (dirty) {
    disablePPM();
    EEPROM.put(0,txPeer);
//    EEPROM.commit();
    dirty = false;
    enablePPM();
  }
  if (bindEnabled) {
    if(millis()-startTime > BIND_TIMEOUT) {
      bindEnabled = false;
    }
  }
}
 
void initRX(){

  startTime = millis();
  
  EEPROM.begin(sizeof(txPeer));
  EEPROM.get(0,txPeer);

  if (!WifiEspNow.begin()) {
    Serial.println("WifiEspNow.begin() failed");
    return;
  }

  WifiEspNow.onReceive(recv_cb, nullptr);

  if (!WifiEspNow.addPeer(txPeer.mac, txPeer.channel)) {
    Serial.printf("WifiEspNow.addPeer() failed MAC: %s", mac2str(txPeer.mac));

  }
  if (!WifiEspNow.addPeer(broadcast_mac, txPeer.channel)) {
    Serial.printf("WifiEspNow.addPeer() failed: MAC: %s", mac2str(broadcast_mac));

  }
}
