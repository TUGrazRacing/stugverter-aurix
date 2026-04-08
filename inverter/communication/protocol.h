#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

/* Defines */
#define PROTOCOL_START_BYTE        0xAA
#define PROTOCOL_MAX_PAYLOAD       126

/* Protocol Commands */
#define PROTOCOL_CMD_READ          0x01
#define PROTOCOL_CMD_WRITE         0x02
#define PROTOCOL_CMD_STREAM_START  0x03
#define PROTOCOL_CMD_STREAM_STOP   0x04
#define PROTOCOL_CMD_DICTIONARY    0x05
#define PROTOCOL_CMD_EVENT         0x0F
#define PROTOCOL_CMD_NACK          0x7F

/* Acknowledge Commands */
#define PROTOCOL_CMD_READ_ACK        0x81
#define PROTOCOL_CMD_WRITE_ACK       0x82
#define PROTOCOL_CMD_STREAM_ACK      0x83
#define PROTOCOL_CMD_STREAM_STOP_ACK 0x84
#define PROTOCOL_CMD_DICTIONARY_ACK  0x85

/* NACK Error Codes */
#define NACK_ERR_BAD_CRC             0x01
#define NACK_ERR_UNKNOWN_CMD         0x02
#define NACK_ERR_INVALID_ADDR        0x03
#define NACK_ERR_OVERTEMP            0x04
#define NACK_ERR_OTHER               0x05

/* Function Prototypes */
void Protocol_Init(void);
void Protocol_Process(void);
void Protocol_SendStreamData(uint8_t stream_id, const uint8_t *data, uint8_t data_len);

#endif // PROTOCOL_H
