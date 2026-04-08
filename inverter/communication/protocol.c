#include "protocol.h"
#include "IfxAsclin_Asc.h"
#include <string.h>
#include "parameter.h"
#include "stream.h"

/* External declaration for your UART instance */
extern IfxAsclin_Asc g_asclin;

/* RX State Machine States */
typedef enum {
    STATE_WAIT_SYNC,
    STATE_WAIT_LENGTH,
    STATE_WAIT_PAYLOAD
} ProtocolState_t;

static ProtocolState_t rx_state;
static uint8_t rx_len;
static uint8_t rx_idx;
static uint8_t rx_buf[PROTOCOL_MAX_PAYLOAD + 4]; // Buffer for CMD + DATA + CRC16

/* Helper Functions */
static void Protocol_ParseByte(uint8_t b);
static void Protocol_ProcessPacket(void);
static void Protocol_SendPacket(uint8_t cmd, const uint8_t *payload, uint8_t payload_len);
static void Protocol_SendNACK(uint8_t reason);
static uint16_t CRC16_CCITT_Update(uint16_t crc, uint8_t data);

void Protocol_Init(void)
{
    rx_state = STATE_WAIT_SYNC;
    rx_len = 0;
    rx_idx = 0;
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
            // Maximale Puffergröße beachten (rx_buf ist PROTOCOL_MAX_PAYLOAD + 4)
            if (rx_len < 3 || rx_len > (PROTOCOL_MAX_PAYLOAD + 3)) {
                rx_state = STATE_WAIT_SYNC;
            } else {
                rx_state = STATE_WAIT_PAYLOAD;
            }
            break;

        case STATE_WAIT_PAYLOAD:
            rx_buf[rx_idx++] = b;

            if (rx_idx >= rx_len) {
                uint16_t calc_crc = 0xFFFF;
                calc_crc = CRC16_CCITT_Update(calc_crc, rx_len);

                for (uint8_t i = 0; i < rx_len - 2; i++) {
                    calc_crc = CRC16_CCITT_Update(calc_crc, rx_buf[i]);
                }

                uint16_t recv_crc = (uint16_t)rx_buf[rx_len - 2] | ((uint16_t)rx_buf[rx_len - 1] << 8);

                if (calc_crc == recv_crc) {
                    Protocol_ProcessPacket();
                } else {
                    Protocol_SendNACK(NACK_ERR_BAD_CRC);
                }
                // RESET für das nächste Paket
                rx_idx = 0;
                rx_state = STATE_WAIT_SYNC;
            }
            break;
    }
}

static void Protocol_ProcessPacket(void)
{
    uint8_t cmd = rx_buf[0];
    uint8_t data_len = rx_len - 3; // Total length minus CMD and 2 bytes CRC

    /* -----------------------------------------------------------------
     * READ COMMAND (0x01)
     * -----------------------------------------------------------------*/
    if (cmd == PROTOCOL_CMD_READ)
    {
        if (data_len < 3) {
            Protocol_SendNACK(NACK_ERR_UNKNOWN_CMD);
            return;
        }

        uint16_t start_addr = (uint16_t)rx_buf[1] | ((uint16_t)rx_buf[2] << 8);
        uint8_t cnt = rx_buf[3];

        uint8_t ack_payload[PROTOCOL_MAX_PAYLOAD];
        uint8_t ack_idx = 0;

        ack_payload[ack_idx++] = start_addr & 0xFF;
        ack_payload[ack_idx++] = (start_addr >> 8) & 0xFF;

        uint8_t cnt_idx = ack_idx++; // Reserve space for the actual count of read registers
        uint8_t actual_cnt = 0;
        uint16_t current_addr = start_addr;

        // If CNT is 0, read all possible up to buffer limit. Otherwise read CNT.
        uint8_t target_cnt = (cnt == 0) ? 255 : cnt;

        for (uint8_t i = 0; i < target_cnt; i++) {
            uint8_t val[8];
            uint8_t vlen = 0;

            if (readParameter(current_addr, val, &vlen)) {
                if ((ack_idx + vlen) <= (PROTOCOL_MAX_PAYLOAD - 2)) {
                    memcpy(&ack_payload[ack_idx], val, vlen);
                    ack_idx += vlen;
                    actual_cnt++;
                    current_addr++; // Increment address for next register
                } else {
                    break; // Packet full
                }
            } else {
                if (cnt != 0) break; // Stop if explicit read fails
            }
        }

        ack_payload[cnt_idx] = actual_cnt;

        if (actual_cnt == 0 && cnt != 0) {
            Protocol_SendNACK(NACK_ERR_INVALID_ADDR);
        } else {
            Protocol_SendPacket(PROTOCOL_CMD_READ_ACK, ack_payload, ack_idx);
        }
    }
    /* -----------------------------------------------------------------
     * WRITE COMMAND (0x02)
     * -----------------------------------------------------------------*/
    else if (cmd == PROTOCOL_CMD_WRITE)
    {
        if (data_len < 4) { Protocol_SendNACK(NACK_ERR_UNKNOWN_CMD); return; }

        uint16_t start_addr = (uint16_t)rx_buf[1] | ((uint16_t)rx_buf[2] << 8);
        uint8_t cnt = rx_buf[3];
        uint8_t actual_cnt = 0;
        uint8_t payload_ptr = 4;

        for (uint8_t i = 0; i < cnt; i++) {
            uint8_t vlen = 0;
            // Erwartete Länge aus der Tabelle holen
            if (getParameterLen(start_addr + i, &vlen)) {
                if (payload_ptr + vlen <= rx_len - 2) {
                    if (writeParameter(start_addr + i, &rx_buf[payload_ptr], vlen)) {
                        actual_cnt++;
                    }
                    payload_ptr += vlen;
                } else { break; } // Paket zu Ende
            } else { break; } // Adresse nicht in Tabelle
        }

        if (actual_cnt == 0 && cnt > 0) {
            Protocol_SendNACK(NACK_ERR_INVALID_ADDR);
        } else {
            uint8_t ack_payload[3];
            ack_payload[0] = start_addr & 0xFF;
            ack_payload[1] = (start_addr >> 8) & 0xFF;
            ack_payload[2] = actual_cnt;
            Protocol_SendPacket(PROTOCOL_CMD_WRITE_ACK, ack_payload, 3);
        }
    }
    /* -----------------------------------------------------------------
     * STREAM START COMMAND (0x03)
     * -----------------------------------------------------------------*/
    else if (cmd == PROTOCOL_CMD_STREAM_START)
    {
        // Expected: ID(1) + FREQ_LSB(1) + FREQ_MSB(1) + ADDR1_LSB(1) ...
        if (data_len < 5) { Protocol_SendNACK(NACK_ERR_UNKNOWN_CMD); return; }

        uint8_t stream_id = rx_buf[1];
        uint16_t freq_x100 = (uint16_t)rx_buf[2] | ((uint16_t)rx_buf[3] << 8);

        // Berechne Anzahl Register (Data_Len - ID(1) - Freq(2)) / 2
        uint8_t num_regs = (data_len - 3) / 2;
        uint16_t regs[STREAM_MAX_REGS];

        if (num_regs > STREAM_MAX_REGS) {
            num_regs = STREAM_MAX_REGS; // Truncate to avoid buffer overflow
        }

        for (uint8_t i = 0; i < num_regs; i++) {
            regs[i] = (uint16_t)rx_buf[4 + (i * 2)] | ((uint16_t)rx_buf[5 + (i * 2)] << 8);
        }

        if (Stream_Start(stream_id, freq_x100, regs, num_regs)) {
            uint8_t ack_payload[PROTOCOL_MAX_PAYLOAD];
            uint8_t ack_idx = 0;
            ack_payload[ack_idx++] = stream_id;

            // Optional: Anhängen der Initialwerte, wie im neuen ACK Format gewünscht
            for (uint8_t i = 0; i < num_regs; i++) {
                uint8_t val[8];
                uint8_t vlen = 0;
                if (readParameter(regs[i], val, &vlen)) {
                    if ((ack_idx + vlen) <= (PROTOCOL_MAX_PAYLOAD - 2)) {
                        memcpy(&ack_payload[ack_idx], val, vlen);
                        ack_idx += vlen;
                    }
                }
            }
            Protocol_SendPacket(PROTOCOL_CMD_STREAM_ACK, ack_payload, ack_idx);
        } else {
            Protocol_SendNACK(NACK_ERR_INVALID_ADDR);
        }
    }
    /* -----------------------------------------------------------------
     * STREAM STOP COMMAND (0x04)
     * -----------------------------------------------------------------*/
    else if (cmd == PROTOCOL_CMD_STREAM_STOP)
    {
        if (data_len < 1) { Protocol_SendNACK(NACK_ERR_UNKNOWN_CMD); return; }
        uint8_t stream_id = rx_buf[1];
        Stream_Stop(stream_id);
        uint8_t ack_payload[1] = { stream_id };
        Protocol_SendPacket(PROTOCOL_CMD_STREAM_STOP_ACK, ack_payload, 1);
    }
    /* -----------------------------------------------------------------
     * DICTIONARY COMMAND (0x05)
     * -----------------------------------------------------------------*/
    else if (cmd == PROTOCOL_CMD_DICTIONARY)
            {
                // --- KORREKTUR: Payload startet direkt nach dem CMD-Byte ---
                uint8_t payload_len = rx_len - 3; // Gesamtlänge minus CMD (1) und CRC (2)
                uint8_t *payload = &rx_buf[1];    // Payload startet bei Index 1
                // ------------------------------------------------------------

                // 1. Längen-Check
                if (payload_len < 3)
                {
                    Protocol_SendNACK(NACK_ERR_OTHER);
                }
                else
                {
                    uint16_t start_addr = payload[0] | (payload[1] << 8);
                    uint8_t req_cnt = payload[2];

                    uint8_t tx_payload[PROTOCOL_MAX_PAYLOAD];
                    uint8_t tx_idx = 0;

                    // Header des Antworten-Pakets: Start Address + tatsächlicher Cnt
                    tx_payload[tx_idx++] = start_addr & 0xFF;
                    tx_payload[tx_idx++] = (start_addr >> 8) & 0xFF;

                    // Platzhalter für den tatsächlichen Count (wird am Ende ausgefüllt)
                    uint8_t count_idx = tx_idx;
                    tx_payload[tx_idx++] = 0;

                    uint8_t actual_cnt = 0;
                    bool found_start = false;

                    // Durch alle Parameter iterieren
                    uint16_t param_count = getParameterCount();

                    for (uint16_t i = 0; i < param_count; i++)
                    {
                        uint16_t p_addr;
                        uint8_t p_type, p_access;
                        const char *p_label;

                        if (getParameterInfo(i, &p_addr, &p_type, &p_access, &p_label))
                        {
                            // Starte ab der gesuchten Adresse oder der nächsthöheren
                            if (p_addr >= start_addr)
                            {
                                found_start = true;
                            }

                            if (found_start)
                            {
                                uint8_t label_len = strlen(p_label);

                                // Absicherung: Laut Protokoll haben wir nur 4 Bits (max 15 Zeichen)
                                if (label_len > 15) {
                                    label_len = 15;
                                }

                                // Prüfen, ob der nächste Parameter noch in den Payload passt
                                if ((tx_idx + 3 + label_len) > PROTOCOL_MAX_PAYLOAD)
                                {
                                    break;
                                }

                                // 1. Parameter Adresse (ID)
                                tx_payload[tx_idx++] = p_addr & 0xFF;
                                tx_payload[tx_idx++] = (p_addr >> 8) & 0xFF;

                                // 2. Info Byte zusammenbauen
                                uint8_t info_byte = (p_type & 0x07) | ((label_len & 0x0F) << 3) | ((p_access & 0x01) << 7);
                                tx_payload[tx_idx++] = info_byte;

                                // 3. ASCII Label
                                memcpy(&tx_payload[tx_idx], p_label, label_len);
                                tx_idx += label_len;

                                actual_cnt++;

                                // Abbrechen, wenn wir die gewünschte Menge erreicht haben
                                if (req_cnt != 0 && actual_cnt >= req_cnt)
                                {
                                    break;
                                }
                            }
                        }
                    }

                    // Tatsächliche Anzahl eintragen
                    tx_payload[count_idx] = actual_cnt;

                    // Dictionary ACK senden
                    Protocol_SendPacket(PROTOCOL_CMD_DICTIONARY_ACK, tx_payload, tx_idx);
                }
            }
        else
        {
            // Unbekanntes Kommando
            Protocol_SendNACK(NACK_ERR_UNKNOWN_CMD);
        }
}

/* Transmits an error frame */
static void Protocol_SendNACK(uint8_t reason)
{
    uint8_t payload[1] = { reason };
    Protocol_SendPacket(PROTOCOL_CMD_NACK, payload, 1);
}

static void Protocol_SendPacket(uint8_t cmd, const uint8_t *payload, uint8_t payload_len)
{
    uint8_t tx_buf[PROTOCOL_MAX_PAYLOAD + 5];
    uint8_t tx_len = 1 + payload_len + 2; // CMD + DATA + CRC16
    uint16_t crc = 0xFFFF; // CCITT Initial Value
    uint8_t total_idx = 0;

    tx_buf[total_idx++] = PROTOCOL_START_BYTE;
    tx_buf[total_idx++] = tx_len;

    // Length is part of the CRC calculation
    crc = CRC16_CCITT_Update(crc, tx_len);

    tx_buf[total_idx++] = cmd;
    crc = CRC16_CCITT_Update(crc, cmd);

    for (uint8_t i = 0; i < payload_len; i++) {
        tx_buf[total_idx++] = payload[i];
        crc = CRC16_CCITT_Update(crc, payload[i]);
    }

    tx_buf[total_idx++] = crc & 0xFF; // CRC LSB
    tx_buf[total_idx++] = (crc >> 8) & 0xFF; // CRC MSB

    Ifx_SizeT count = total_idx;
    IfxAsclin_Asc_write(&g_asclin, tx_buf, &count, 0xFFFFFFFF);
}

/* Standard CRC16-CCITT implementation with strict 16-bit masking */
static uint16_t CRC16_CCITT_Update(uint16_t crc, uint8_t data)
{
    crc = (uint16_t)((crc ^ ((uint16_t)data << 8)) & 0xFFFF);
    for (uint8_t i = 0; i < 8; i++) {
        if (crc & 0x8000) {
            crc = (uint16_t)(((crc << 1) ^ 0x1021) & 0xFFFF);
        } else {
            crc = (uint16_t)((crc << 1) & 0xFFFF);
        }
    }
    return crc;
}



/**
 * @brief Sendet ein Stream-Datenpaket (CMD 0x83)
 * @param stream_id Die ID des Streams
 * @param data Pointer auf die rohen Parameter-Daten
 * @param data_len Länge der Parameter-Daten in Bytes
 */
void Protocol_SendStreamData(uint8_t stream_id, const uint8_t *data, uint8_t data_len)
{
    uint8_t tx_payload[PROTOCOL_MAX_PAYLOAD];

    // Sicherheitscheck: Verhindern, dass wir über den Puffer hinausschreiben
    if ((1 + data_len) > PROTOCOL_MAX_PAYLOAD) {
        return; // Payload zu groß
    }

    // 1. Stream ID als erstes Byte
    tx_payload[0] = stream_id;

    // 2. Die eigentlichen Parameter-Daten anhängen (falls vorhanden)
    if (data_len > 0 && data != NULL) {
        memcpy(&tx_payload[1], data, data_len);
    }

    // 3. Als CMD_STREAM_ACK (0x83) an den PC senden
    Protocol_SendPacket(PROTOCOL_CMD_STREAM_ACK, tx_payload, 1 + data_len);
}
