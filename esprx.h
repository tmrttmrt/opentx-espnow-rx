#ifndef ESPRX_H
#define ESPRX_H
#include "esprc_packet.h"
const uint8_t broadcast_mac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
extern volatile uint32_t packRecv;
extern volatile uint32_t packAckn;
extern volatile uint32_t recvPeriod;
extern int16_t locChannelOutputs[MAX_OUTPUT_CHANNELS];

void initRX();
void initPPM();
#endif
