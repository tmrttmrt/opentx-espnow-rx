#ifndef ESPRX_H
#define ESPRX_H
#define MAX_OUTPUT_CHANNELS 32

const uint8_t broadcast_mac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

enum PacketType_t {
  DATA,
  TELE,
  BIND,
  ACK
};

typedef struct {
  PacketType_t type:4;
  uint8_t idx:4;
  uint16_t crc;
  uint16_t ch[MAX_OUTPUT_CHANNELS];
} __attribute__((packed)) TXPacket_t;

typedef struct {
  PacketType_t type:4;
  uint8_t idx:4;
  uint16_t crc;
} __attribute__((packed)) RXPacket_t;

extern volatile uint32_t packRecv;
extern volatile uint32_t packAckn;
extern volatile uint32_t recvPeriod;
extern int16_t locChannelOutputs[MAX_OUTPUT_CHANNELS];

void recv_cb(const uint8_t mac[6], const uint8_t* buf, size_t count, void* cbarg);
void initRX();
#endif
