#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#define PROTOCOL_START_BYTE_0       0xAA
#define PROTOCOL_START_BYTE_1       0x55
#define PROTOCOL_TCP_HEADER_SIZE    5U

#define PROTOCOL_CMD_DICTIONARY     0x00
#define PROTOCOL_CMD_READ           0x01
#define PROTOCOL_CMD_READ_ACK       0x81
#define PROTOCOL_CMD_WRITE          0x02
#define PROTOCOL_CMD_WRITE_ACK      0x82
#define PROTOCOL_CMD_STREAM_START   0x03
#define PROTOCOL_CMD_STREAM_ACK     0x83
#define PROTOCOL_CMD_STREAM_STOP    0x04
#define PROTOCOL_CMD_STREAM_STOP_ACK 0x84
#define PROTOCOL_CMD_DICTIONARY_ACK 0x80
#define PROTOCOL_CMD_CONFIG_COMMIT  0x06
#define PROTOCOL_CMD_CONFIG_COMMIT_ACK 0x86
#define PROTOCOL_CMD_ERROR          0x8F

#define PROTOCOL_ERROR_INVALID_ADDR  0x01
#define PROTOCOL_ERROR_ACCESS_DENIED 0x02
#define PROTOCOL_ERROR_OTHER         0x03

#define PROTOCOL_MAX_PAYLOAD        250
#define PROTOCOL_UDP_STREAM_PORT    3040U

typedef void (*Protocol_TxBytesFn)(const uint8_t *data, uint16_t len);

/* Function Prototypes */
void Protocol_Init(void);
void Protocol_SetTxHandler(Protocol_TxBytesFn handler);
void Protocol_ProcessBytes(const uint8_t *data, uint16_t len);
void Protocol_NetworkInit(void);
bool Protocol_HasUdpSender(void);
void Protocol_SendStreamBatch(uint8_t stream_id, uint8_t sample_count, const uint8_t *samples, uint16_t samples_len);

#endif
