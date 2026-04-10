#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#define PROTOCOL_START_BYTE         0xAA

#define PROTOCOL_CMD_READ           0x01
#define PROTOCOL_CMD_READ_ACK       0x81
#define PROTOCOL_CMD_WRITE          0x02
#define PROTOCOL_CMD_WRITE_ACK      0x82
#define PROTOCOL_CMD_STREAM_START   0x03
#define PROTOCOL_CMD_STREAM_ACK     0x83
#define PROTOCOL_CMD_STREAM_STOP    0x04
#define PROTOCOL_CMD_STREAM_STOP_ACK 0x84
#define PROTOCOL_CMD_DICTIONARY     0x05
#define PROTOCOL_CMD_DICTIONARY_ACK 0x85
#define PROTOCOL_CMD_NACK           0x7F

#define PROTOCOL_NACK_BAD_CRC       0x01
#define PROTOCOL_NACK_UNKNOWN_CMD   0x02
#define PROTOCOL_NACK_INVALID_ADDR  0x03
#define PROTOCOL_NACK_READ_ONLY     0x04
#define PROTOCOL_NACK_OTHER         0x05

#define PROTOCOL_MAX_PAYLOAD        250

/* Function Prototypes */
void Protocol_Init(void);
void Protocol_Process(void);
void Protocol_SendStreamData(uint8_t stream_id, const uint8_t *data, uint8_t data_len);

#endif
