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

/* Internal Helper Functions */
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
      rx_len = b; // Length = CMD(1) + DATA(N) + CRC(2)
      rx_idx = 0;
      // Min length: CMD(1) + DATA(0) + CRC(2) = 3
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
        // CRC Calculation over Start Byte + Length + Payload (CMD + DATA)
        uint16_t calc_crc = 0xFFFF;
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
          Protocol_SendNACK(PROTOCOL_NACK_BAD_CRC);
        }
        rx_state = STATE_WAIT_SYNC;
      }
      break;
  }
}

static void Protocol_ProcessPacket (void)
{
  uint8_t cmd = rx_buf[0];
  uint8_t data_len = rx_len - 3; // Total minus CMD and CRC bytes

  // --- 4.1 READ REGISTER (0x01) ---
  if (cmd == PROTOCOL_CMD_READ)
  {
    uint8_t req_cnt = data_len / 2;
    if (req_cnt == 0 || (data_len % 2 != 0))
    {
      Protocol_SendNACK(PROTOCOL_NACK_OTHER);
      return;
    }

    uint8_t ack_payload[PROTOCOL_MAX_PAYLOAD];
    uint8_t ack_idx = 0;

    for (uint8_t i = 0; i < req_cnt; i++)
    {
      uint16_t addr = (uint16_t) rx_buf[1 + (i * 2)] | ((uint16_t) rx_buf[2 + (i * 2)] << 8);
      uint8_t val[8];
      uint8_t vlen = 0;

      if (readParameter(addr, val, &vlen))
      {
        // Fragmentation: if this entry exceeds buffer, send current chunk
        if ((ack_idx + 2 + vlen) > PROTOCOL_MAX_PAYLOAD)
        {
          Protocol_SendPacket(PROTOCOL_CMD_READ_ACK, ack_payload, ack_idx);
          ack_idx = 0;
        }
        ack_payload[ack_idx++] = addr & 0xFF;
        ack_payload[ack_idx++] = (addr >> 8) & 0xFF;
        memcpy(&ack_payload[ack_idx], val, vlen);
        ack_idx += vlen;
      }
      else
      {
        Protocol_SendNACK(PROTOCOL_NACK_INVALID_ADDR);
        return;
      }
    }
    if (ack_idx > 0)
      Protocol_SendPacket(PROTOCOL_CMD_READ_ACK, ack_payload, ack_idx);
  }

  // --- 4.2 WRITE REGISTER (0x02) ---
  else if (cmd == PROTOCOL_CMD_WRITE)
  {
    if (data_len < 2)
    {
      Protocol_SendNACK(PROTOCOL_NACK_OTHER);
      return;
    }

    uint16_t start_addr = (uint16_t) rx_buf[1] | ((uint16_t) rx_buf[2] << 8);
    uint8_t ptr = 1;
    uint8_t count = 0;

    while (ptr < data_len + 1)
    {
      uint16_t addr = (uint16_t) rx_buf[ptr] | ((uint16_t) rx_buf[ptr + 1] << 8);
      uint8_t vlen = 0;
      if (getParameterLen(addr, &vlen))
      {
        if (writeParameter(addr, &rx_buf[ptr + 2], vlen))
        {
          count++;
          ptr += (2 + vlen);
        }
        else
        {
          Protocol_SendNACK(PROTOCOL_NACK_READ_ONLY);
          return;
        }
      }
      else
      {
        Protocol_SendNACK(PROTOCOL_NACK_INVALID_ADDR);
        return;
      }
    }

    uint8_t ack[3] = {start_addr & 0xFF, (start_addr >> 8) & 0xFF, count};
    Protocol_SendPacket(PROTOCOL_CMD_WRITE_ACK, ack, 3);
  }

  // --- 4.4 STREAM START (0x03) ---
  else if (cmd == PROTOCOL_CMD_STREAM_START)
  {
    if (data_len < 3)
    { // ID(1) + Freq(2)
      Protocol_SendNACK(PROTOCOL_NACK_OTHER);
      return;
    }
    uint8_t id = rx_buf[1];
    uint16_t freq = (uint16_t) rx_buf[2] | ((uint16_t) rx_buf[3] << 8);
    uint8_t num_regs = (data_len - 3) / 2;
    uint16_t regs[STREAM_MAX_REGS];

    for (uint8_t i = 0; i < num_regs && i < STREAM_MAX_REGS; i++)
    {
      regs[i] = (uint16_t) rx_buf[4 + (i * 2)] | ((uint16_t) rx_buf[5 + (i * 2)] << 8);
    }

    if (Stream_Start(id, freq, regs, num_regs))
    {
      // Initial data response (0x83)
      uint8_t ack[PROTOCOL_MAX_PAYLOAD];
      ack[0] = id;
      uint8_t ack_idx = 1;
      for (uint8_t i = 0; i < num_regs; i++)
      {
        uint8_t val[8], vlen;
        if (readParameter(regs[i], val, &vlen))
        {
          memcpy(&ack[ack_idx], val, vlen);
          ack_idx += vlen;
        }
      }
      Protocol_SendPacket(PROTOCOL_CMD_STREAM_ACK, ack, ack_idx);
    }
    else
    {
      Protocol_SendNACK(PROTOCOL_NACK_INVALID_ADDR);
    }
  }

  // --- 4.4 STREAM STOP (0x04) ---
  else if (cmd == PROTOCOL_CMD_STREAM_STOP)
  {
    uint8_t id = rx_buf[1];
    Stream_Stop(id);
    Protocol_SendPacket(PROTOCOL_CMD_STREAM_STOP_ACK, &id, 1);
  }

  // --- 4.3 DICTIONARY (0x05) ---
  else if (cmd == PROTOCOL_CMD_DICTIONARY)
  {
    uint16_t start_addr = rx_buf[1] | (rx_buf[2] << 8);
    uint8_t req_cnt = rx_buf[3];
    uint16_t total_params = getParameterCount();
    uint16_t i = 0;

    for (; i < total_params; i++)
    {
      uint16_t addr;
      if (getParameterInfo(i, &addr, NULL, NULL, NULL, NULL) && addr >= start_addr)
        break;
    }

    uint8_t remaining = (req_cnt == 0) ? (total_params - i) : req_cnt;

    while (remaining > 0 && i < total_params)
    {
      uint8_t tx_p[PROTOCOL_MAX_PAYLOAD];
      uint16_t chunk_addr;
      getParameterInfo(i, &chunk_addr, NULL, NULL, NULL, NULL);

      tx_p[0] = chunk_addr & 0xFF;
      tx_p[1] = (chunk_addr >> 8) & 0xFF;
      uint8_t count_idx = 2;
      uint8_t tx_idx = 3;
      uint8_t actual_cnt = 0;

      while (remaining > 0 && i < total_params)
      {
        uint16_t p_addr;
        uint8_t p_type, p_access, p_unit[2];
        const char *p_label;
        if (getParameterInfo(i, &p_addr, &p_type, &p_access, &p_label, p_unit))
        {
          uint8_t l_len = (uint8_t) strlen(p_label);
          if (l_len > 15)
            l_len = 15;

          if ((tx_idx + 5 + l_len) > PROTOCOL_MAX_PAYLOAD)
            break;

          tx_p[tx_idx++] = p_addr & 0xFF;
          tx_p[tx_idx++] = (p_addr >> 8) & 0xFF;
          // Info Byte: Access(7), LabelLen(3-6), Type(0-2)
          tx_p[tx_idx++] = (p_type & 0x07) | ((l_len & 0x0F) << 3) | ((p_access & 0x01) << 7);
          tx_p[tx_idx++] = p_unit[0];
          tx_p[tx_idx++] = p_unit[1];
          memcpy(&tx_p[tx_idx], p_label, l_len);
          tx_idx += l_len;
          actual_cnt++;
          remaining--;
        }
        i++;
      }
      tx_p[count_idx] = actual_cnt;
      Protocol_SendPacket(PROTOCOL_CMD_DICTIONARY_ACK, tx_p, tx_idx);
    }
  }
  else
  {
    Protocol_SendNACK(PROTOCOL_NACK_UNKNOWN_CMD);
  }
}

static void Protocol_SendPacket (uint8_t cmd, const uint8_t *payload, uint8_t payload_len)
{
  uint8_t tx_buf[PROTOCOL_MAX_PAYLOAD + 5];
  uint16_t crc = 0xFFFF;
  uint8_t len = 1 + payload_len + 2; // CMD + DATA + CRC
  uint8_t idx = 0;

  tx_buf[idx++] = PROTOCOL_START_BYTE;
  crc = CRC16_CCITT_Update(crc, PROTOCOL_START_BYTE);

  tx_buf[idx++] = len;
  crc = CRC16_CCITT_Update(crc, len);

  tx_buf[idx++] = cmd;
  crc = CRC16_CCITT_Update(crc, cmd);

  for (uint8_t i = 0; i < payload_len; i++)
  {
    tx_buf[idx++] = payload[i];
    crc = CRC16_CCITT_Update(crc, payload[i]);
  }

  tx_buf[idx++] = crc & 0xFF;
  tx_buf[idx++] = (crc >> 8) & 0xFF;

  Ifx_SizeT count = idx;
  IfxAsclin_Asc_write(&g_asclin, tx_buf, &count, 0xFFFFFFFF);
}

static void Protocol_SendNACK (uint8_t reason)
{
  Protocol_SendPacket(PROTOCOL_CMD_NACK, &reason, 1);
}

static uint16_t CRC16_CCITT_Update (uint16_t crc, uint8_t data)
{
  crc = (uint16_t) ((crc ^ ((uint16_t) data << 8)) & 0xFFFF);
  for (uint8_t i = 0; i < 8; i++)
  {
    if (crc & 0x8000)
      crc = (uint16_t) (((crc << 1) ^ 0x1021) & 0xFFFF);
    else
      crc = (uint16_t) ((crc << 1) & 0xFFFF);
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
