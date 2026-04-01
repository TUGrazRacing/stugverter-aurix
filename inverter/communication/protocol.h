#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

/* Defines (Tune these if you already have them defined elsewhere) */
#define PROTOCOL_START_BYTE        0xAA
#define PROTOCOL_MAX_PAYLOAD       64

/* Protocol Commands */
#define PROTOCOL_CMD_READ          0x01
#define PROTOCOL_CMD_WRITE         0x02
#define PROTOCOL_CMD_READ_ACK      0x81
#define PROTOCOL_CMD_WRITE_ACK     0x82
#define PROTOCOL_CMD_ERROR         0xFF

/* New Streaming Commands */
#define PROTOCOL_CMD_STREAM_START  0x03
#define PROTOCOL_CMD_STREAM_STOP   0x04
#define PROTOCOL_CMD_STREAM_DATA   0x05

/* Function Prototypes */
void Protocol_Init(void);
void Protocol_Process(void);

/* Called by stream.c to push live data out */
void Protocol_SendStreamData(uint8_t stream_id, const uint8_t *data, uint8_t data_len);

#endif /* PROTOCOL_H */
