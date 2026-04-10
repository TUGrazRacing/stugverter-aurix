#include "protocol.h"
#include "IfxAsclin_Asc.h"
#include <string.h>
#include "parameter.h"
#include "stream.h"

/* External declaration for your UART instance */
extern IfxAsclin_Asc g_asclin;

/* RX State Machine States */
typedef enum
{
  STATE_WAIT_SYNC, STATE_WAIT_LENGTH, STATE_WAIT_PAYLOAD
} ProtocolState_t;

static ProtocolState_t rx_state;
static uint8_t rx_len;
static uint8_t rx_idx;
static uint8_t rx_buf[PROTOCOL_MAX_PAYLOAD + 4]; // Buffer for CMD + DATA + CRC16

/* Helper Functions */
static void Protocol_ParseByte (uint8_t b);
static void Protocol_ProcessPacket (void);
static void Protocol_SendPacket (uint8_t cmd, const uint8_t *payload, uint8_t payload_len);
static void Protocol_SendNACK (uint8_t reason);
static uint16_t CRC16_CCITT_Update (uint16_t crc, uint8_t data);

void Protocol_Init (void)
{
  rx_state = STATE_WAIT_SYNC;
  rx_len = 0;
  rx_idx = 0;
}

void Protocol_Process (void)
{
  uint8_t rxData;
  Ifx_SizeT count = 1;
  while (IfxAsclin_Asc_read(&g_asclin, &rxData, &count, 0) && count > 0)
  {
    Protocol_ParseByte(rxData);
    count = 1;
  }
}

static void Protocol_ParseByte (uint8_t b)
{
  switch (rx_state)
  {
    case STATE_WAIT_SYNC :
      if (b == PROTOCOL_START_BYTE)
      {
        rx_state = STATE_WAIT_LENGTH;
      }
      break;

    case STATE_WAIT_LENGTH :
      rx_len = b;
      rx_idx = 0;
      // Maximale Puffergröße beachten (rx_buf ist PROTOCOL_MAX_PAYLOAD + 4)
      if (rx_len < 3 || rx_len > (PROTOCOL_MAX_PAYLOAD + 3))
      {
        rx_state = STATE_WAIT_SYNC;
      }
      else
      {
        rx_state = STATE_WAIT_PAYLOAD;
      }
      break;

    case STATE_WAIT_PAYLOAD :
      rx_buf[rx_idx++] = b;

      if (rx_idx >= rx_len)
      {
        uint16_t calc_crc = 0xFFFF;

        // The Qt host includes the Start Byte in the CRC!
        calc_crc = CRC16_CCITT_Update(calc_crc, PROTOCOL_START_BYTE);
        calc_crc = CRC16_CCITT_Update(calc_crc, rx_len);

        for (uint8_t i = 0; i < rx_len - 2; i++)
        {
          calc_crc = CRC16_CCITT_Update(calc_crc, rx_buf[i]);
        }

        uint16_t recv_crc = (uint16_t) rx_buf[rx_len - 2] | ((uint16_t) rx_buf[rx_len - 1] << 8);

        if (calc_crc == recv_crc)
        {
          Protocol_ProcessPacket();
        }
        else
        {
          Protocol_SendNACK(NACK_ERR_BAD_CRC);
        }
        // RESET für das nächste Paket
        rx_idx = 0;
        rx_state = STATE_WAIT_SYNC;
      }
      break;
  }
}

static void Protocol_ProcessPacket (void)
{
  uint8_t cmd = rx_buf[0];
  uint8_t data_len = rx_len - 3; // Total length minus CMD and 2 bytes CRC

  /* -----------------------------------------------------------------
     * READ COMMAND (0x01)
     * Erwartet: [ID1_LSB] [ID1_MSB] [ID2_LSB] [ID2_MSB] ... (Liste von 16-Bit Adressen)
     * Antwort:  [Val1_Bytes...] [Val2_Bytes...] ... (Reine Werte in exakter Reihenfolge)
     * -----------------------------------------------------------------*/
    if (cmd == PROTOCOL_CMD_READ)
    {
        // Da jede ID/Adresse 2 Bytes lang ist, ergibt sich die Anzahl aus der Payload-Länge
        uint8_t req_cnt = data_len / 2;

        // Wenn keine IDs mitgeschickt wurden oder die Länge ungerade ist
        if (req_cnt == 0 || (data_len % 2 != 0))
        {
            Protocol_SendNACK(NACK_ERR_OTHER);
            return;
        }

        uint8_t ack_payload[PROTOCOL_MAX_PAYLOAD];
        uint8_t ack_idx = 0;

        // Iteriere in der korrekten Reihenfolge über alle angeforderten Adressen
        for (uint8_t i = 0; i < req_cnt; i++)
        {
            // Extrahiere die 2-Byte Adresse (Little Endian). rx_buf[1] ist das erste Byte nach dem CMD.
            uint16_t req_addr = (uint16_t)rx_buf[1 + (i * 2)] | ((uint16_t)rx_buf[2 + (i * 2)] << 8);

            uint8_t val[8]; // Temporärer Puffer für den Wert (max 8 Bytes laut Parameter-Typen)
            uint8_t vlen = 0;

            if (readParameter(req_addr, val, &vlen))
            {
                if ((ack_idx + vlen) <= PROTOCOL_MAX_PAYLOAD)
                {
                    memcpy(&ack_payload[ack_idx], val, vlen);
                    ack_idx += vlen;
                }
                else
                {
                    Protocol_SendNACK(NACK_ERR_OTHER);
                    return;
                }
            }
            else
            {
                Protocol_SendNACK(NACK_ERR_INVALID_ADDR);
                return;
            }
        }

        Protocol_SendPacket(PROTOCOL_CMD_READ_ACK, ack_payload, ack_idx);
    }
    /* -----------------------------------------------------------------
       * WRITE COMMAND (0x02)
       * Erwartet: [Addr1_LSB][Addr1_MSB][Val1...] [Addr2_LSB][Addr2_MSB][Val2...] ...
       * Antwort:  [Anzahl_erfolgreicher_Writes]
       * -----------------------------------------------------------------*/
      else if (cmd == PROTOCOL_CMD_WRITE)
      {
        uint8_t payload_ptr = 1; // rx_buf[0] ist CMD, Daten starten ab Index 1
        uint8_t actual_cnt = 0;

        // Wir loopen durch die Payload, solange noch mindestens Platz für eine Adresse (2 Bytes) ist
        while (payload_ptr < data_len + 1)
        {
          // 1. Adresse extrahieren (2 Bytes, Little Endian)
          if (payload_ptr + 2 > data_len + 1) break; // Paket zu Ende
          uint16_t req_addr = (uint16_t)rx_buf[payload_ptr] | ((uint16_t)rx_buf[payload_ptr + 1] << 8);
          payload_ptr += 2;

          // 2. Erwartete Länge für dieses spezifische Register aus der Tabelle holen
          uint8_t vlen = 0;
          if (getParameterLen(req_addr, &vlen))
          {
            // 3. Prüfen, ob der Wert für dieses Register noch komplett im Puffer liegt
            if (payload_ptr + vlen <= data_len + 1)
            {
              // 4. Parameter schreiben
              if (writeParameter(req_addr, &rx_buf[payload_ptr], vlen))
              {
                actual_cnt++;
              }
              // Pointer um die Länge des geschriebenen Wertes verschieben
              payload_ptr += vlen;
            }
            else
            {
              // Paket abgeschnitten, Wert unvollständig
              break;
            }
          }
          else
          {
            // Adresse existiert nicht -> Wir brechen ab und senden NACK,
            // da wir nicht wissen, wie viele Bytes wir für den "unbekannten" Wert überspringen müssten.
            Protocol_SendNACK(NACK_ERR_INVALID_ADDR);
            return;
          }
        }

        // Wenn absolut nichts geschrieben werden konnte (bei angefordertem Write) -> NACK
        if (actual_cnt == 0 && data_len > 0)
        {
          Protocol_SendNACK(NACK_ERR_OTHER);
        }
        else
        {
          // Sende ein ACK mit der Anzahl der erfolgreich verarbeiteten Register
          uint8_t ack_payload[1];
          ack_payload[0] = actual_cnt;
          Protocol_SendPacket(PROTOCOL_CMD_WRITE_ACK, ack_payload, 1);
        }
      }
  /* -----------------------------------------------------------------
   * STREAM START COMMAND (0x03)
   * -----------------------------------------------------------------*/
  else if (cmd == PROTOCOL_CMD_STREAM_START)
  {
    // Expected: ID(1) + FREQ_LSB(1) + FREQ_MSB(1) + ADDR1_LSB(1) ...
    if (data_len < 5)
    {
      Protocol_SendNACK(NACK_ERR_UNKNOWN_CMD);
      return;
    }

    uint8_t stream_id = rx_buf[1];
    uint16_t freq_x100 = (uint16_t) rx_buf[2] | ((uint16_t) rx_buf[3] << 8);

    // Berechne Anzahl Register (Data_Len - ID(1) - Freq(2)) / 2
    uint8_t num_regs = (data_len - 3) / 2;
    uint16_t regs[STREAM_MAX_REGS];

    if (num_regs > STREAM_MAX_REGS)
    {
      num_regs = STREAM_MAX_REGS; // Truncate to avoid buffer overflow
    }

    for (uint8_t i = 0; i < num_regs; i++)
    {
      regs[i] = (uint16_t) rx_buf[4 + (i * 2)] | ((uint16_t) rx_buf[5 + (i * 2)] << 8);
    }

    if (Stream_Start(stream_id, freq_x100, regs, num_regs))
    {
      uint8_t ack_payload[PROTOCOL_MAX_PAYLOAD];
      uint8_t ack_idx = 0;
      ack_payload[ack_idx++] = stream_id;

      // Optional: Anhängen der Initialwerte, wie im neuen ACK Format gewünscht
      for (uint8_t i = 0; i < num_regs; i++)
      {
        uint8_t val[8];
        uint8_t vlen = 0;
        if (readParameter(regs[i], val, &vlen))
        {
          if ((ack_idx + vlen) <= (PROTOCOL_MAX_PAYLOAD - 2))
          {
            memcpy(&ack_payload[ack_idx], val, vlen);
            ack_idx += vlen;
          }
        }
      }
      Protocol_SendPacket(PROTOCOL_CMD_STREAM_ACK, ack_payload, ack_idx);
    }
    else
    {
      Protocol_SendNACK(NACK_ERR_INVALID_ADDR);
    }
  }
  /* -----------------------------------------------------------------
   * STREAM STOP COMMAND (0x04)
   * -----------------------------------------------------------------*/
  else if (cmd == PROTOCOL_CMD_STREAM_STOP)
  {
    if (data_len < 1)
    {
      Protocol_SendNACK(NACK_ERR_UNKNOWN_CMD);
      return;
    }
    uint8_t stream_id = rx_buf[1];
    Stream_Stop(stream_id);
    uint8_t ack_payload[1] = {stream_id};
    Protocol_SendPacket(PROTOCOL_CMD_STREAM_STOP_ACK, ack_payload, 1);
  }
  /* -----------------------------------------------------------------
   * DICTIONARY COMMAND (0x05)
   * -----------------------------------------------------------------*/
  else if (cmd == PROTOCOL_CMD_DICTIONARY)
  {
    uint8_t payload_len = rx_len - 3;
    uint8_t *payload = &rx_buf[1];

    if (payload_len < 3)
    {
      Protocol_SendNACK(NACK_ERR_OTHER);
      return;
    }

    uint16_t start_addr = payload[0] | (payload[1] << 8);
    uint8_t req_cnt = payload[2];

    uint16_t param_count = getParameterCount();
    uint16_t i = 0;

    // 1. Find the starting parameter index safely
    for (; i < param_count; i++)
    {
      uint16_t p_addr;
      // Pass NULL for parameters we don't care about right now
      if (getParameterInfo(i, &p_addr, NULL, NULL, NULL, NULL) && p_addr >= start_addr)
      {
        break;
      }
    }

    // If no parameters match or start address is beyond table bounds
    if (i >= param_count)
    {
      uint8_t tx_payload[3] = { start_addr & 0xFF, (start_addr >> 8) & 0xFF, 0 };
      Protocol_SendPacket(PROTOCOL_CMD_DICTIONARY_ACK, tx_payload, 3);
      return;
    }

    uint8_t remaining_cnt = (req_cnt == 0) ? (param_count - i) : req_cnt;

    // 2. Loop to build and send packets until all requested parameters are transmitted
    while (remaining_cnt > 0 && i < param_count)
    {
      uint8_t tx_payload[PROTOCOL_MAX_PAYLOAD];
      uint8_t tx_idx = 0;

      uint16_t chunk_start_addr;
      getParameterInfo(i, &chunk_start_addr, NULL, NULL, NULL, NULL);

      // Packet Header: Current Chunk Start Address
      tx_payload[tx_idx++] = chunk_start_addr & 0xFF;
      tx_payload[tx_idx++] = (chunk_start_addr >> 8) & 0xFF;

      uint8_t count_idx = tx_idx++; // Reserve byte for actual count
      uint8_t actual_cnt = 0;

      while (remaining_cnt > 0 && i < param_count)
      {
        uint16_t p_addr;
        uint8_t p_type, p_access;
        const char *p_label;
        uint8_t p_unit[2];

        // Retrieve everything, including the new 2-byte unit array
        if (getParameterInfo(i, &p_addr, &p_type, &p_access, &p_label, p_unit))
        {
          uint8_t label_len = strlen(p_label);
          if (label_len > 15) label_len = 15;

          // Check if it fits: ID(2) + Info(1) + Unit(2) + Label(label_len)
          if ((tx_idx + 5 + label_len) > PROTOCOL_MAX_PAYLOAD)
          {
            break; // Chunk is full, break inner loop to send
          }

          // Address
          tx_payload[tx_idx++] = p_addr & 0xFF;
          tx_payload[tx_idx++] = (p_addr >> 8) & 0xFF;

          // Info Byte
          uint8_t info_byte = (p_type & 0x07) | ((label_len & 0x0F) << 3) | ((p_access & 0x01) << 7);
          tx_payload[tx_idx++] = info_byte;

          // Units (Appended directly matching the new format)
          tx_payload[tx_idx++] = p_unit[0];
          tx_payload[tx_idx++] = p_unit[1];

          // Label
          memcpy(&tx_payload[tx_idx], p_label, label_len);
          tx_idx += label_len;

          actual_cnt++;
          remaining_cnt--;
        }
        i++;
      }

      // Fill in actual count for this specific chunk and transmit
      tx_payload[count_idx] = actual_cnt;
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
static void Protocol_SendNACK (uint8_t reason)
{
  uint8_t payload[1] = {reason};
  Protocol_SendPacket(PROTOCOL_CMD_NACK, payload, 1);
}

static void Protocol_SendPacket (uint8_t cmd, const uint8_t *payload, uint8_t payload_len)
{
  uint8_t tx_buf[PROTOCOL_MAX_PAYLOAD + 5];
  uint8_t tx_len = 1 + payload_len + 2; // CMD + DATA + CRC16
  uint16_t crc = 0xFFFF; // CCITT Initial Value
  uint8_t total_idx = 0;

  tx_buf[total_idx++] = PROTOCOL_START_BYTE;

  // Include the Start Byte in the outgoing CRC
  crc = CRC16_CCITT_Update(crc, PROTOCOL_START_BYTE);

  tx_buf[total_idx++] = tx_len;

  // Length is part of the CRC calculation
  crc = CRC16_CCITT_Update(crc, tx_len);

  tx_buf[total_idx++] = cmd;
  crc = CRC16_CCITT_Update(crc, cmd);

  for (uint8_t i = 0; i < payload_len; i++)
  {
    tx_buf[total_idx++] = payload[i];
    crc = CRC16_CCITT_Update(crc, payload[i]);
  }

  tx_buf[total_idx++] = crc & 0xFF; // CRC LSB
  tx_buf[total_idx++] = (crc >> 8) & 0xFF; // CRC MSB

  Ifx_SizeT count = total_idx;
  IfxAsclin_Asc_write(&g_asclin, tx_buf, &count, 0xFFFFFFFF);
}

/* Standard CRC16-CCITT implementation with strict 16-bit masking */
static uint16_t CRC16_CCITT_Update (uint16_t crc, uint8_t data)
{
  crc = (uint16_t) ((crc ^ ((uint16_t) data << 8)) & 0xFFFF);
  for (uint8_t i = 0; i < 8; i++)
  {
    if (crc & 0x8000)
    {
      crc = (uint16_t) (((crc << 1) ^ 0x1021) & 0xFFFF);
    }
    else
    {
      crc = (uint16_t) ((crc << 1) & 0xFFFF);
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
void Protocol_SendStreamData (uint8_t stream_id, const uint8_t *data, uint8_t data_len)
{
  uint8_t tx_payload[PROTOCOL_MAX_PAYLOAD];

  // Sicherheitscheck: Verhindern, dass wir über den Puffer hinausschreiben
  if ((1 + data_len) > PROTOCOL_MAX_PAYLOAD)
  {
    return; // Payload zu groß
  }

  // 1. Stream ID als erstes Byte
  tx_payload[0] = stream_id;

  // 2. Die eigentlichen Parameter-Daten anhängen (falls vorhanden)
  if (data_len > 0 && data != NULL)
  {
    memcpy(&tx_payload[1], data, data_len);
  }

  // 3. Als CMD_STREAM_ACK (0x83) an den PC senden
  Protocol_SendPacket(PROTOCOL_CMD_STREAM_ACK, tx_payload, 1 + data_len);
}
