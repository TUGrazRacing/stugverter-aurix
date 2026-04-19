#ifndef STREAM_H
#define STREAM_H

#include <stdbool.h>
#include <stdint.h>
#include "protocol.h"

#define STREAM_MAX_STREAMS 4U
#define STREAM_MAX_REGS    16U
#define STREAM_FRAME_BUFFER_SIZE 8U
#define STREAM_FRAME_HEADER_BYTES 14U
#define STREAM_FRAME_VALUE_BYTES (PROTOCOL_MAX_PAYLOAD - STREAM_FRAME_HEADER_BYTES)

void Stream_Init(void);
bool Stream_Start(uint8_t id, uint16_t freq_x100, const uint16_t *regs, uint8_t num_regs);
bool Stream_Stop(uint8_t id);
void Stream_Process(void);
void Stream_TransmitPending(void);

#endif
