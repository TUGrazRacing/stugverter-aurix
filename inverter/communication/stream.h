#ifndef STREAM_H
#define STREAM_H

#include <stdbool.h>
#include <stdint.h>
#include "protocol.h"

#define STREAM_MAX_STREAMS 4U
#define STREAM_MAX_REGS    16U
#define STREAM_PACKET_BUFFER_SIZE 16U
#define STREAM_PACKET_HEADER_BYTES 4U
#define STREAM_SAMPLE_HEADER_BYTES 10U
#define STREAM_PACKET_PAYLOAD_BYTES (PROTOCOL_UDP_MAX_PAYLOAD - STREAM_PACKET_HEADER_BYTES)
#define STREAM_FRAME_VALUE_BYTES (PROTOCOL_MAX_PAYLOAD - STREAM_PACKET_HEADER_BYTES - STREAM_SAMPLE_HEADER_BYTES)

void Stream_Init(void);

bool Stream_Start(uint8_t id, uint8_t loop_divider, const uint16_t *regs, uint8_t num_regs);
bool Stream_Stop(uint8_t id);
void Stream_OnControlLoop(void);
void Stream_TransmitPending(void);

#endif
