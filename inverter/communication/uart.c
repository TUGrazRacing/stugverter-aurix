#include "uart.h"
#include "params.h"
#include "IfxAsclin_bf.h"
#include "IfxAsclin_PinMap.h" // Required for the hardware pin definitions
#include <string.h>
#include <io.h>

/* --- Hardware Configuration --- */
typedef struct {
    Ifx_ASCLIN *const asclin;
    const IfxAsclin_Tx_Out *tx_pin;
    const IfxAsclin_Rx_In *rx_pin;
} UartHwConfig_t;

static const UartHwConfig_t UART_HW = {
  .asclin = &MODULE_ASCLIN0,
  .tx_pin = &IfxAsclin0_TX_P14_0_OUT,
  .rx_pin = &IfxAsclin0_RXA_P14_1_IN
};

char uart_print_buf[SERIALIO_STRLEN];

/* --- Protocol Definitions --- */
#define START_BYTE   0xAA
#define CMD_READ     0x01
#define CMD_WRITE    0x02
#define CMD_ACK      0x80
#define CMD_NACK     0x81
#define MAX_PAYLOAD  128

/* --- NACK Error Codes --- */
#define ERR_INVALID_LEN    0x01
#define ERR_ADDR_NOT_FOUND 0x02
#define ERR_WRITE_FAILED   0x03 // E.g., wrong payload size for the data type
#define ERR_CRC_MISMATCH   0x04
#define ERR_UNKNOWN_CMD    0x05

typedef enum {
    STATE_WAIT_START,
    STATE_WAIT_LEN,
    STATE_WAIT_PAYLOAD,
    STATE_WAIT_CRC_H,
    STATE_WAIT_CRC_L
} UartState_t;

/* --- Static Variables --- */
static UartState_t rx_state = STATE_WAIT_START;
static uint8_t rx_len       = 0;
static uint8_t rx_index     = 0;
static uint8_t rx_buf[MAX_PAYLOAD];
static uint16_t rx_crc      = 0;

/* --- Static Helper Functions --- */

/* Safely writes a byte to the TX FIFO, preventing buffer overflow */
static void UART_WriteTx(uint8_t c) {
    while (IfxAsclin_getTxFifoFillLevelFlagStatus(UART_HW.asclin) != TRUE) {}
    IfxAsclin_clearTxFifoFillLevelFlag(UART_HW.asclin);
    IfxAsclin_writeTxData(UART_HW.asclin, c);
}

/* Standard CCITT-FALSE CRC16 */
static uint16_t Calculate_CRC16(const uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    return crc;
}

static void Send_Response(uint8_t cmd, const uint8_t* payload, uint8_t len) {
    uint8_t frame_len = 1 + len; // Command + Payload
    uint8_t crc_buf[256];

    crc_buf[0] = frame_len;
    crc_buf[1] = cmd;
    if (len > 0) {
        memcpy(&crc_buf[2], payload, len);
    }

    uint16_t crc = Calculate_CRC16(crc_buf, frame_len + 1);

    /* Send using the safe TX helper */
    UART_WriteTx(START_BYTE);
    UART_WriteTx(frame_len);
    UART_WriteTx(cmd);

    for(uint8_t i = 0; i < len; i++) {
        UART_WriteTx(payload[i]);
    }

    UART_WriteTx((uint8_t)(crc >> 8));
    UART_WriteTx((uint8_t)(crc & 0xFF));
}

static void Process_Message(void) {
    uint8_t cmd = rx_buf[0];
    uint8_t err_code = 0;

    if (cmd == CMD_READ) {
        // Expected payload: Addr_H, Addr_L (Total length = 3)
        if (rx_len != 3) {
            err_code = ERR_INVALID_LEN;
            Send_Response(CMD_NACK, &err_code, 1);
            return;
        }

        uint16_t address = (rx_buf[1] << 8) | rx_buf[2];
        uint8_t out_data[8]; // Sized to 8 to support uint64
        uint8_t out_len = 0;

        if (Param_Read(address, out_data, &out_len)) {
            // Respond with ACK + Address + Data
            uint8_t resp_payload[10];
            resp_payload[0] = rx_buf[1]; // Addr_H
            resp_payload[1] = rx_buf[2]; // Addr_L
            memcpy(&resp_payload[2], out_data, out_len);
            Send_Response(CMD_ACK, resp_payload, 2 + out_len);
        } else {
            err_code = ERR_ADDR_NOT_FOUND;
            Send_Response(CMD_NACK, &err_code, 1);
        }
    }
    else if (cmd == CMD_WRITE) {
        // Expected payload: Addr_H, Addr_L, Data... (Total length > 3)
        if (rx_len < 4) {
            err_code = ERR_INVALID_LEN;
            Send_Response(CMD_NACK, &err_code, 1);
            return;
        }

        uint16_t address = (rx_buf[1] << 8) | rx_buf[2];
        uint8_t data_len = rx_len - 3; // Total length - CMD(1) - Addr(2)

        if (Param_Write(address, &rx_buf[3], data_len)) {
            // Respond with ACK + Address (to confirm successful write)
            uint8_t resp_payload[2] = { rx_buf[1], rx_buf[2] };
            Send_Response(CMD_ACK, resp_payload, 2);
        } else {
            // Write failed (either address not found or payload size mismatch)
            err_code = ERR_WRITE_FAILED;
            Send_Response(CMD_NACK, &err_code, 1);
        }
    }
    else {
        err_code = ERR_UNKNOWN_CMD;
        Send_Response(CMD_NACK, &err_code, 1);
    }
}

static void UART_ParseByte(uint8_t byte) {
    switch (rx_state) {
        case STATE_WAIT_START:
            if (byte == START_BYTE) {
                rx_state = STATE_WAIT_LEN;
            }
            break;

        case STATE_WAIT_LEN:
            rx_len = byte;
            rx_index = 0;
            if (rx_len > 0 && rx_len < MAX_PAYLOAD) {
                rx_state = STATE_WAIT_PAYLOAD;
            } else {
                rx_state = STATE_WAIT_START; // Invalid length
            }
            break;

        case STATE_WAIT_PAYLOAD:
            rx_buf[rx_index++] = byte;
            if (rx_index >= rx_len) {
                rx_state = STATE_WAIT_CRC_H;
            }
            break;

        case STATE_WAIT_CRC_H:
            rx_crc = byte << 8;
            rx_state = STATE_WAIT_CRC_L;
            break;

        case STATE_WAIT_CRC_L:
            rx_crc |= byte;

            // Validate CRC over [Length, Payload...]
            uint8_t crc_buf[256];
            crc_buf[0] = rx_len;
            memcpy(&crc_buf[1], rx_buf, rx_len);

            uint16_t calc_crc = Calculate_CRC16(crc_buf, rx_len + 1);

            if (calc_crc == rx_crc) {
                Process_Message();
            } else {
                // Send a NACK specifically for CRC mismatch so you know the packet arrived but failed validation
                uint8_t err_code = ERR_CRC_MISMATCH;
                Send_Response(CMD_NACK, &err_code, 1);
            }
            rx_state = STATE_WAIT_START;
            break;
    }
}
/* --- Public Function Implementations --- */

void UART_Init(sint32 baudrate)
{
    /* Create an instance of the ASC handle */
    IfxAsclin_Asc ascHandle;

    /* Initialize an instance of IfxAsclin_Asc_Config with default values */
    IfxAsclin_Asc_Config ascConfig;
    IfxAsclin_Asc_initModuleConfig(&ascConfig, UART_HW.asclin);

    ascConfig.baudrate.baudrate = (float32)baudrate;

    /* Pin configuration */
    const IfxAsclin_Asc_Pins pins = {
        NULL_PTR, IfxPort_InputMode_pullUp,
        UART_HW.rx_pin, IfxPort_InputMode_pullUp,
        NULL_PTR, IfxPort_OutputMode_pushPull,
        UART_HW.tx_pin, IfxPort_OutputMode_pushPull,
        IfxPort_PadDriver_cmosAutomotiveSpeed1};
    ascConfig.pins = &pins;

    /* Initialize module with above parameters */
    IfxAsclin_Asc_initModule(&ascHandle, &ascConfig);

    /* Set the Transmit FIFO Level Flag (TFL) via the FLAGSSET register to start the transmission */
    UART_HW.asclin->FLAGSSET.B.TFLS = 1;
}

void UART_Process(void) {
    /* Process all bytes currently in the RX FIFO non-blockingly */
    while (IfxAsclin_getRxFifoFillLevelFlagStatus(UART_HW.asclin) == TRUE) {
        IfxAsclin_clearRxFifoFillLevelFlag(UART_HW.asclin);
        uint8_t c = IfxAsclin_readRxData(UART_HW.asclin);
        UART_ParseByte(c);
    }
}

/* --- IO Standard Library Hooks --- */

void _io_putc (int c, struct _io *io)
{
    if (io->fp == NULL)
    {
        /* Called on print on string */
        if (io->ptr < io->end)
        {
            *(io->ptr++) = (char)c;
        }
    }
    else
    {
        /* Insert CR before LF */
        if (c == '\n')
        {
             UART_WriteTx('\r');
        }

        UART_WriteTx((uint8_t)c);
    }
}

int _io_getc(struct _io *io)
{
    if (io->fp != NULL)
    {
        while (IfxAsclin_getRxFifoFillLevelFlagStatus(UART_HW.asclin) != TRUE)
        {}

        IfxAsclin_clearRxFifoFillLevelFlag(UART_HW.asclin);
        return IfxAsclin_readRxData(UART_HW.asclin);
    }
    else
    {
        __debug();
    }
    return EOF;
}
