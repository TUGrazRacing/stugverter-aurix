#include "protocol.h"
#include "IfxAsclin_Asc.h"
#include <string.h>
#include "parameter.h"
#include "stream.h" /* Included for streaming integration */

/* External declaration for your UART instance */
extern IfxAsclin_Asc g_asclin;

/* RX State Machine States */
typedef enum {
    STATE_WAIT_SYNC,
    STATE_WAIT_LENGTH,
    STATE_WAIT_PAYLOAD,
    STATE_WAIT_CRC
} ProtocolState_t;

#define STREAM_MAX_REGS 32

static ProtocolState_t rx_state;
static uint8_t rx_len;
static uint8_t rx_idx;
static uint8_t rx_crc;
static uint8_t rx_buf[3 + PROTOCOL_MAX_PAYLOAD];

/* Helper Functions */
static void Protocol_ParseByte(uint8_t b);
static void Protocol_ProcessPacket(void);
static void Protocol_SendPacket(uint8_t cmd, uint16_t addr, const uint8_t *payload, uint8_t payload_len);
static uint8_t CRC8_Update(uint8_t crc, uint8_t data);

void Protocol_Init(void)
{
    rx_state = STATE_WAIT_SYNC;
    rx_len = 0;
    rx_idx = 0;
    rx_crc = 0;
}

void Protocol_Process(void)
{
    uint8_t rxData;
    Ifx_SizeT count = 1;

    while (IfxAsclin_Asc_read(&g_asclin, &rxData, &count, 0) && count > 0)
    {
        Protocol_ParseByte(rxData);
        count = 1;
    }
}

static void Protocol_ParseByte(uint8_t b)
{
    switch (rx_state)
    {
        case STATE_WAIT_SYNC:
            if (b == PROTOCOL_START_BYTE) {
                rx_state = STATE_WAIT_LENGTH;
            }
            break;

        case STATE_WAIT_LENGTH:
            rx_len = b;
            rx_idx = 0;
            rx_crc = 0;
            rx_crc = CRC8_Update(rx_crc, b);

            /* Length bounds check. Minimum is 1 (Command only). */
            if (rx_len < 1 || rx_len > sizeof(rx_buf)) {
                rx_state = STATE_WAIT_SYNC;
            } else {
                rx_state = STATE_WAIT_PAYLOAD;
            }
            break;

        case STATE_WAIT_PAYLOAD:
            rx_buf[rx_idx++] = b;
            rx_crc = CRC8_Update(rx_crc, b);

            if (rx_idx >= rx_len) {
                rx_state = STATE_WAIT_CRC;
            }
            break;

        case STATE_WAIT_CRC:
            if (b == rx_crc) {
                Protocol_ProcessPacket();
            }
            rx_state = STATE_WAIT_SYNC;
            break;

        default:
            rx_state = STATE_WAIT_SYNC;
            break;
    }
}

static void Protocol_ProcessPacket(void)
{
    uint8_t cmd = rx_buf[0];

    /* -----------------------------------------------------------------
     * LEGACY READ / WRITE COMMANDS
     * -----------------------------------------------------------------*/
    if (cmd == PROTOCOL_CMD_READ || cmd == PROTOCOL_CMD_WRITE)
    {
        if (rx_len < 3) {
            Protocol_SendPacket(PROTOCOL_CMD_ERROR, 0, NULL, 0);
            return;
        }

        uint16_t addr = (uint16_t)rx_buf[1] | ((uint16_t)rx_buf[2] << 8);
        uint8_t *payload = &rx_buf[3];
        uint8_t payload_len = rx_len - 3;

        if (cmd == PROTOCOL_CMD_READ) {
            uint8_t out_data[PROTOCOL_MAX_PAYLOAD];
            uint8_t out_len = 0;
            if (readParameter(addr, out_data, &out_len)) {
                Protocol_SendPacket(PROTOCOL_CMD_READ_ACK, addr, out_data, out_len);
            } else {
                Protocol_SendPacket(PROTOCOL_CMD_ERROR, addr, NULL, 0);
            }
        }
        else if (cmd == PROTOCOL_CMD_WRITE) {
            if (writeParameter(addr, payload, payload_len)) {
                Protocol_SendPacket(PROTOCOL_CMD_WRITE_ACK, addr, NULL, 0);
            } else {
                Protocol_SendPacket(PROTOCOL_CMD_ERROR, addr, NULL, 0);
            }
        }
    }
    /* -----------------------------------------------------------------
     * STREAM START COMMAND
     * -----------------------------------------------------------------*/
    else if (cmd == PROTOCOL_CMD_STREAM_START)
    {
        /* Structure: [Cmd] [ID] [Freq_LSB] [Freq_MSB] [Reg1_LSB] [Reg1_MSB] ... */
        if (rx_len < 4) {
            Protocol_SendPacket(PROTOCOL_CMD_ERROR, 0, NULL, 0);
            return;
        }

        uint8_t stream_id = rx_buf[1];
        uint16_t freq_x100 = (uint16_t)rx_buf[2] | ((uint16_t)rx_buf[3] << 8);
        uint8_t num_regs = (rx_len - 4) / 2;

        uint16_t regs[STREAM_MAX_REGS];
        if (num_regs > STREAM_MAX_REGS) {
            num_regs = STREAM_MAX_REGS; // Truncate to avoid buffer overflow
        }

        for (uint8_t i = 0; i < num_regs; i++) {
            regs[i] = (uint16_t)rx_buf[4 + (i * 2)] | ((uint16_t)rx_buf[5 + (i * 2)] << 8);
        }

        if (Stream_Start(stream_id, freq_x100, regs, num_regs)) {
            Protocol_SendPacket(PROTOCOL_CMD_WRITE_ACK, stream_id, NULL, 0);
        } else {
            Protocol_SendPacket(PROTOCOL_CMD_ERROR, stream_id, NULL, 0);
        }
    }
    /* -----------------------------------------------------------------
     * STREAM STOP COMMAND
     * -----------------------------------------------------------------*/
    else if (cmd == PROTOCOL_CMD_STREAM_STOP)
    {
        /* Structure: [Cmd] [ID] */
        if (rx_len < 2) {
            Protocol_SendPacket(PROTOCOL_CMD_ERROR, 0, NULL, 0);
            return;
        }

        uint8_t stream_id = rx_buf[1];
        if (Stream_Stop(stream_id)) {
            Protocol_SendPacket(PROTOCOL_CMD_WRITE_ACK, stream_id, NULL, 0);
        } else {
            Protocol_SendPacket(PROTOCOL_CMD_ERROR, stream_id, NULL, 0);
        }
    }
    else
    {
        Protocol_SendPacket(PROTOCOL_CMD_ERROR, 0, NULL, 0);
    }
}

/* Expose this for Stream.c to transmit live data */
void Protocol_SendStreamData(uint8_t stream_id, const uint8_t *data, uint8_t data_len)
{
    /* Use the existing address field for the stream ID so host can filter it */
    Protocol_SendPacket(PROTOCOL_CMD_STREAM_DATA, (uint16_t)stream_id, data, data_len);
}

static void Protocol_SendPacket(uint8_t cmd, uint16_t addr, const uint8_t *payload, uint8_t payload_len)
{
    uint8_t tx_buf[3 + PROTOCOL_MAX_PAYLOAD + 2];
    uint8_t tx_len = 3 + payload_len;
    uint8_t crc = 0;
    uint8_t total_idx = 0;

    tx_buf[total_idx++] = PROTOCOL_START_BYTE;

    tx_buf[total_idx++] = tx_len;
    crc = CRC8_Update(crc, tx_len);

    tx_buf[total_idx++] = cmd;
    crc = CRC8_Update(crc, cmd);

    uint8_t addr_lsb = addr & 0xFF;
    uint8_t addr_msb = (addr >> 8) & 0xFF;

    tx_buf[total_idx++] = addr_lsb;
    crc = CRC8_Update(crc, addr_lsb);

    tx_buf[total_idx++] = addr_msb;
    crc = CRC8_Update(crc, addr_msb);

    for (uint8_t i = 0; i < payload_len; i++) {
        tx_buf[total_idx++] = payload[i];
        crc = CRC8_Update(crc, payload[i]);
    }

    tx_buf[total_idx++] = crc;

    Ifx_SizeT send_count = total_idx;
    IfxAsclin_Asc_write(&g_asclin, tx_buf, &send_count, TIME_INFINITE);
}

static uint8_t CRC8_Update(uint8_t crc, uint8_t data)
{
    crc ^= data;
    for (uint8_t i = 0; i < 8; i++) {
        if (crc & 0x80) {
            crc = (crc << 1) ^ 0x07;
        } else {
            crc = (crc << 1);
        }
    }
    return crc;
}
