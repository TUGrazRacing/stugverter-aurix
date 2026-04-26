#include "protocol.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"
#include "UART_Logging.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "parameter.h"
#include "stream.h"
#include "app_config.h"

#define PROTOCOL_DEBUG_ENABLE 1
#define PROTOCOL_LOG_BUFFER_SIZE 192U
#define PROTOCOL_DICTIONARY_GROUP_ALL 0xFFU

static struct udp_pcb *stream_udp_pcb;
static ip_addr_t stream_destination;
static Protocol_TxBytesFn protocol_tx_handler;

static void Protocol_ProcessPacket(uint8_t cmd, const uint8_t *payload, uint16_t payload_len);
static void Protocol_SendPacket(uint8_t cmd, const uint8_t *payload, uint16_t payload_len);
static void Protocol_SendError(uint16_t address, uint8_t reason);
static bool Protocol_SendUdpPayload(const uint8_t *payload, uint16_t payload_len);
static void Protocol_Log(const char *fmt, ...);
static const char *Protocol_CmdName(uint8_t cmd);
static const char *Protocol_ErrorName(uint8_t reason);

void Protocol_Init(void)
{
  protocol_tx_handler = NULL;
  Protocol_Log("PROTOCOL: init\r\n");
}

void Protocol_SetTxHandler(Protocol_TxBytesFn handler)
{
  protocol_tx_handler = handler;
  Protocol_Log("PROTOCOL: tx handler %s\r\n", (handler != NULL) ? "attached" : "cleared");
}

void Protocol_NetworkInit(void)
{
  Protocol_Log("PROTOCOL: init UDP sender on port %u\r\n", (unsigned int)PROTOCOL_UDP_STREAM_PORT);

  if (stream_udp_pcb != NULL)
  {
    Protocol_Log("PROTOCOL: UDP sender already ready\r\n");
    return;
  }

  stream_udp_pcb = udp_new();
  if (stream_udp_pcb == NULL)
  {
    Protocol_Log("PROTOCOL: udp_new failed\r\n");
    return;
  }

  IP4_ADDR(&stream_destination, 255, 255, 255, 255);
  if (udp_bind(stream_udp_pcb, IP_ADDR_ANY, PROTOCOL_UDP_STREAM_PORT) != ERR_OK)
  {
    Protocol_Log("PROTOCOL: udp_bind failed\r\n");
    udp_remove(stream_udp_pcb);
    stream_udp_pcb = NULL;
    return;
  }

  Protocol_Log("PROTOCOL: UDP sender ready\r\n");
}

bool Protocol_HasUdpSender(void)
{
  return (stream_udp_pcb != NULL);
}

void Protocol_ProcessBytes(const uint8_t *data, uint16_t len)
{
  uint16_t payload_len;
  uint8_t cmd;

  if ((data == NULL) || (len < PROTOCOL_TCP_HEADER_SIZE))
  {
    return;
  }

  if ((data[0] != PROTOCOL_START_BYTE_0) || (data[1] != PROTOCOL_START_BYTE_1))
  {
    Protocol_Log("PROTOCOL: bad start bytes 0x%02X 0x%02X\r\n", (unsigned int)data[0], (unsigned int)data[1]);
    Protocol_SendError(0U, PROTOCOL_ERROR_OTHER);
    return;
  }

  cmd = data[2];
  payload_len = (uint16_t)data[3] | ((uint16_t)data[4] << 8);

  if (payload_len > PROTOCOL_MAX_PAYLOAD)
  {
    Protocol_Log("PROTOCOL: payload too large %u\r\n", (unsigned int)payload_len);
    Protocol_SendError(0U, PROTOCOL_ERROR_OTHER);
    return;
  }

  if (len != (uint16_t)(PROTOCOL_TCP_HEADER_SIZE + payload_len))
  {
    Protocol_Log("PROTOCOL: frame length mismatch len=%u expected=%u\r\n",
                 (unsigned int)len,
                 (unsigned int)(PROTOCOL_TCP_HEADER_SIZE + payload_len));
    Protocol_SendError(0U, PROTOCOL_ERROR_OTHER);
    return;
  }

  Protocol_Log("PROTOCOL: process cmd=%s(0x%02X) payload=%u\r\n", Protocol_CmdName(cmd), (unsigned int)cmd, (unsigned int)payload_len);
  Protocol_ProcessPacket(cmd, &data[PROTOCOL_TCP_HEADER_SIZE], payload_len);
}

static void Protocol_ProcessPacket(uint8_t cmd, const uint8_t *payload, uint16_t payload_len)
{
  /* Command dispatcher: validate payload first, then execute register/state action. */
  if (cmd == PROTOCOL_CMD_READ)
  {
    if ((payload_len == 0U) || ((payload_len % 2U) != 0U))
    {
      Protocol_SendError(0U, PROTOCOL_ERROR_OTHER);
      return;
    }

    uint16_t req_cnt = payload_len / 2U;
    Protocol_Log("PROTOCOL: read req count=%u\r\n", (unsigned int)req_cnt);
    uint8_t ack_payload[PROTOCOL_MAX_PAYLOAD];
    uint16_t ack_idx = 0U;

    for (uint16_t i = 0U; i < req_cnt; i++)
    {
      uint16_t addr = (uint16_t)payload[i * 2U] | ((uint16_t)payload[(i * 2U) + 1U] << 8);
      uint8_t val[8];
      uint8_t vlen = 0U;

      if (!readParameter(addr, val, &vlen))
      {
        Protocol_SendError(addr, PROTOCOL_ERROR_INVALID_ADDR);
        return;
      }

      if ((ack_idx + vlen) > PROTOCOL_MAX_PAYLOAD)
      {
        Protocol_SendError(addr, PROTOCOL_ERROR_OTHER);
        return;
      }

      memcpy(&ack_payload[ack_idx], val, vlen);
      ack_idx = (uint16_t)(ack_idx + vlen);
    }

    Protocol_SendPacket(PROTOCOL_CMD_READ_ACK, ack_payload, ack_idx);
    Protocol_Log("PROTOCOL: read ack bytes=%u\r\n", (unsigned int)ack_idx);
    return;
  }

  if (cmd == PROTOCOL_CMD_WRITE)
  {
    uint16_t ptr = 0U;
    uint8_t count = 0U;
    uint16_t first_addr = 0U;
    Protocol_Log("PROTOCOL: write req bytes=%u\r\n", (unsigned int)payload_len);

    if (payload_len < 2U)
    {
      Protocol_SendError(0U, PROTOCOL_ERROR_OTHER);
      return;
    }

    while ((ptr + 2U) <= payload_len)
    {
      uint16_t addr = (uint16_t)payload[ptr] | ((uint16_t)payload[ptr + 1U] << 8);
      uint8_t vlen = 0U;

      if (count == 0U)
      {
        first_addr = addr;
      }

      if (!getParameterLen(addr, &vlen))
      {
        Protocol_SendError(addr, PROTOCOL_ERROR_INVALID_ADDR);
        return;
      }

      if ((uint16_t)(ptr + 2U + vlen) > payload_len)
      {
        Protocol_SendError(addr, PROTOCOL_ERROR_OTHER);
        return;
      }

      if (!writeParameter(addr, &payload[ptr + 2U], vlen))
      {
        Protocol_SendError(addr, PROTOCOL_ERROR_ACCESS_DENIED);
        return;
      }

      ptr = (uint16_t)(ptr + 2U + vlen);
      count++;
    }

    if (ptr != payload_len)
    {
      Protocol_SendError(0U, PROTOCOL_ERROR_OTHER);
      return;
    }

    {
      uint8_t ack[3];
      ack[0] = (uint8_t)(first_addr & 0xFFU);
      ack[1] = (uint8_t)((first_addr >> 8) & 0xFFU);
      ack[2] = count;
      Protocol_SendPacket(PROTOCOL_CMD_WRITE_ACK, ack, sizeof(ack));
      Protocol_Log("PROTOCOL: write ack first=0x%04X count=%u\r\n", (unsigned int)first_addr, (unsigned int)count);
    }
    return;
  }

  if (cmd == PROTOCOL_CMD_STREAM_START)
  {
    if ((payload_len < 4U) || (((payload_len - 2U) % 2U) != 0U))
    {
      Protocol_SendError(0U, PROTOCOL_ERROR_OTHER);
      return;
    }

    uint8_t stream_id = payload[0];
    uint8_t loop_divider = payload[1];
    uint8_t num_regs = (uint8_t)((payload_len - 2U) / 2U);
    Protocol_Log("PROTOCOL: stream start id=%u divider=%u regs=%u\r\n",
           (unsigned int)stream_id,
           (unsigned int)loop_divider,
           (unsigned int)num_regs);
    uint16_t regs[STREAM_MAX_REGS];

    if ((loop_divider == 0U) || (num_regs == 0U) || (num_regs > STREAM_MAX_REGS))
    {
      Protocol_SendError(0U, PROTOCOL_ERROR_OTHER);
      return;
    }

    for (uint8_t i = 0U; i < num_regs; i++)
    {
      uint16_t base = (uint16_t)(2U + (i * 2U));
      regs[i] = (uint16_t)payload[base] | ((uint16_t)payload[base + 1U] << 8);

      {
        uint8_t vlen = 0U;
        if (!getParameterLen(regs[i], &vlen))
        {
          Protocol_SendError(regs[i], PROTOCOL_ERROR_INVALID_ADDR);
          return;
        }
      }
    }

    /* Reconfigure the same stream ID if it already exists. */
    (void)Stream_Stop(stream_id);

    if (!Stream_Start(stream_id, loop_divider, regs, num_regs))
    {
      Protocol_SendError(0U, PROTOCOL_ERROR_OTHER);
      return;
    }

    {
      uint8_t ack[PROTOCOL_MAX_PAYLOAD];
      uint16_t ack_idx = 0U;
      ack[ack_idx++] = stream_id;
      ack[ack_idx++] = num_regs;
      for (uint8_t i = 0U; i < num_regs; i++)
      {
        ack[ack_idx++] = (uint8_t)(regs[i] & 0xFFU);
        ack[ack_idx++] = (uint8_t)((regs[i] >> 8) & 0xFFU);
      }
      Protocol_SendPacket(PROTOCOL_CMD_STREAM_ACK, ack, ack_idx);
      Protocol_Log("PROTOCOL: stream ack id=%u regs=%u\r\n", (unsigned int)stream_id, (unsigned int)num_regs);
    }
    return;
  }

  if (cmd == PROTOCOL_CMD_STREAM_STOP)
  {
    if (payload_len != 1U)
    {
      Protocol_SendError(0U, PROTOCOL_ERROR_OTHER);
      return;
    }

    uint8_t id = payload[0];
    if (!Stream_Stop(id))
    {
      Protocol_SendError(0U, PROTOCOL_ERROR_OTHER);
      return;
    }

    Protocol_SendPacket(PROTOCOL_CMD_STREAM_STOP_ACK, &id, 1U);
    Protocol_Log("PROTOCOL: stream stop ack id=%u\r\n", (unsigned int)id);
    return;
  }

  if (cmd == PROTOCOL_CMD_CONFIG_COMMIT)
  {
    if (payload_len != 0U)
    {
      Protocol_SendError(0U, PROTOCOL_ERROR_OTHER);
      return;
    }

    commitConfigShadow();
    Protocol_SendPacket(PROTOCOL_CMD_CONFIG_COMMIT_ACK, NULL, 0U);
    Protocol_Log("PROTOCOL: config commit ack\r\n");
    return;
  }

  if (cmd == PROTOCOL_CMD_DICTIONARY)
  {
    if (payload_len != 1U)
    {
      Protocol_SendError(0U, PROTOCOL_ERROR_OTHER);
      return;
    }

    uint8_t group_filter = payload[0];
    uint16_t total_params = getParameterCount();
    uint16_t i = 0U;

    while (i < total_params)
    {
      uint8_t tx_p[PROTOCOL_MAX_PAYLOAD];
      uint16_t tx_idx = 0U;

      while (i < total_params)
      {
        uint16_t p_addr;
        uint8_t p_type;
        uint8_t p_access;
        uint8_t p_group;
        uint8_t p_unit[PARAM_UNIT_LEN];
        const char *p_label;

        if (!getParameterInfo(i, &p_addr, &p_type, &p_access, &p_group, &p_label, p_unit))
        {
          i++;
          continue;
        }

        if ((group_filter != PROTOCOL_DICTIONARY_GROUP_ALL) && (p_group != group_filter))
        {
          i++;
          continue;
        }

        if ((tx_idx + PARAM_DICT_RECORD_LEN) > PROTOCOL_MAX_PAYLOAD)
        {
          break;
        }

        tx_p[tx_idx++] = (uint8_t)(p_addr & 0xFFU);
        tx_p[tx_idx++] = (uint8_t)((p_addr >> 8) & 0xFFU);
        tx_p[tx_idx++] = (uint8_t)((p_type & 0x0FU) | ((p_access & 0x03U) << 4));
        tx_p[tx_idx++] = p_group;

        memset(&tx_p[tx_idx], ' ', PARAM_NAME_LEN);
        if (p_label != NULL)
        {
          uint8_t l_len = (uint8_t)strlen(p_label);
          if (l_len > PARAM_NAME_LEN)
          {
            l_len = PARAM_NAME_LEN;
          }
          memcpy(&tx_p[tx_idx], p_label, l_len);
        }
        tx_idx += PARAM_NAME_LEN;

        memcpy(&tx_p[tx_idx], p_unit, PARAM_UNIT_LEN);
        tx_idx += PARAM_UNIT_LEN;

        i++;
      }

      Protocol_SendPacket(PROTOCOL_CMD_DICTIONARY_ACK, tx_p, tx_idx);
      Protocol_Log("PROTOCOL: dictionary chunk bytes=%u\r\n", (unsigned int)tx_idx);
    }
    return;
  }

  Protocol_SendError(0U, PROTOCOL_ERROR_OTHER);
}

static void Protocol_SendPacket(uint8_t cmd, const uint8_t *payload, uint16_t payload_len)
{
  uint8_t tx_buf[PROTOCOL_TCP_HEADER_SIZE + PROTOCOL_MAX_PAYLOAD];
  uint16_t idx = 0U;

  if (payload_len > PROTOCOL_MAX_PAYLOAD)
  {
    Protocol_Log("PROTOCOL: TX payload too large %u\r\n", (unsigned int)payload_len);
    return;
  }

  tx_buf[idx++] = PROTOCOL_START_BYTE_0;
  tx_buf[idx++] = PROTOCOL_START_BYTE_1;
  tx_buf[idx++] = cmd;
  tx_buf[idx++] = (uint8_t)(payload_len & 0xFFU);
  tx_buf[idx++] = (uint8_t)((payload_len >> 8) & 0xFFU);

  if ((payload_len > 0U) && (payload != NULL))
  {
    memcpy(&tx_buf[idx], payload, payload_len);
    idx = (uint16_t)(idx + payload_len);
  }

  if (protocol_tx_handler != NULL)
  {
    Protocol_Log("PROTOCOL: TX cmd=%s(0x%02X) len=%u\r\n", Protocol_CmdName(cmd), (unsigned int)cmd, (unsigned int)idx);
    protocol_tx_handler(tx_buf, idx);
  }
  else
  {
    Protocol_Log("PROTOCOL: TX dropped, no handler cmd=0x%02X\r\n", (unsigned int)cmd);
  }
}

static void Protocol_SendError(uint16_t address, uint8_t reason)
{
  uint8_t payload[3];
  payload[0] = (uint8_t)(address & 0xFFU);
  payload[1] = (uint8_t)((address >> 8) & 0xFFU);
  payload[2] = reason;

  Protocol_Log("PROTOCOL: ERROR addr=0x%04X reason=%s(0x%02X)\r\n",
               (unsigned int)address,
               Protocol_ErrorName(reason),
               (unsigned int)reason);

  Protocol_SendPacket(PROTOCOL_CMD_ERROR, payload, sizeof(payload));
}

void Protocol_SendStreamData(uint8_t stream_id,
                             uint16_t sequence,
                             uint64_t timestamp_ticks,
                             const uint8_t *data,
                             uint8_t data_len)
{
  uint8_t tx_payload[PROTOCOL_MAX_PAYLOAD];
  uint16_t payload_len = 0U;

  if ((stream_udp_pcb == NULL) || ((uint16_t)(14U + data_len) > PROTOCOL_MAX_PAYLOAD))
  {
    return;
  }

  tx_payload[payload_len++] = PROTOCOL_START_BYTE_0;
  tx_payload[payload_len++] = PROTOCOL_START_BYTE_1;
  tx_payload[payload_len++] = stream_id;
  tx_payload[payload_len++] = 1U;
  tx_payload[payload_len++] = (uint8_t)(sequence & 0xFFU);
  tx_payload[payload_len++] = (uint8_t)((sequence >> 8) & 0xFFU);

  for (uint8_t i = 0U; i < 8U; i++)
  {
    tx_payload[payload_len++] = (uint8_t)((timestamp_ticks >> (8U * i)) & 0xFFU);
  }

  if ((data_len > 0U) && (data != NULL))
  {
    memcpy(&tx_payload[payload_len], data, data_len);
    payload_len = (uint16_t)(payload_len + data_len);
  }

  Protocol_SendUdpPayload(tx_payload, payload_len);
  /*
  Protocol_Log("PROTOCOL: stream UDP id=%u seq=%u ts=%llu count=%u payload=%u\r\n",
               (unsigned int)stream_id,
               (unsigned int)sequence,
               (unsigned long long)timestamp_ticks,
               1U,
               (unsigned int)payload_len);
  */
}

static bool Protocol_SendUdpPayload(const uint8_t *payload, uint16_t payload_len)
{
  struct pbuf *packet = NULL;

  if ((stream_udp_pcb == NULL) || (payload == NULL) || (payload_len == 0U))
  {
    return false;
  }

  packet = pbuf_alloc(PBUF_TRANSPORT, payload_len, PBUF_RAM);
  if (packet == NULL)
  {
    return false;
  }

  if (pbuf_take(packet, payload, payload_len) != ERR_OK)
  {
    pbuf_free(packet);
    return false;
  }

  (void)udp_sendto(stream_udp_pcb, packet, &stream_destination, PROTOCOL_UDP_STREAM_PORT);
  pbuf_free(packet);
  return true;
}

static void Protocol_Log(const char *fmt, ...)
{
#if PROTOCOL_DEBUG_ENABLE
  char buffer[PROTOCOL_LOG_BUFFER_SIZE];
  va_list args;
  int len;

  va_start(args, fmt);
  len = vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  if (len < 0)
  {
    return;
  }

  if ((size_t)len >= sizeof(buffer))
  {
    len = (int)(sizeof(buffer) - 1U);
    buffer[len] = '\0';
  }

  sendUARTMessage(buffer, (Ifx_SizeT)len);
#else
  (void)fmt;
#endif
}

static const char *Protocol_CmdName(uint8_t cmd)
{
  switch (cmd)
  {
    case PROTOCOL_CMD_DICTIONARY: return "DICTIONARY";
    case PROTOCOL_CMD_READ: return "READ";
    case PROTOCOL_CMD_READ_ACK: return "READ_ACK";
    case PROTOCOL_CMD_WRITE: return "WRITE";
    case PROTOCOL_CMD_WRITE_ACK: return "WRITE_ACK";
    case PROTOCOL_CMD_STREAM_START: return "STREAM_START";
    case PROTOCOL_CMD_STREAM_ACK: return "STREAM_ACK";
    case PROTOCOL_CMD_STREAM_STOP: return "STREAM_STOP";
    case PROTOCOL_CMD_STREAM_STOP_ACK: return "STREAM_STOP_ACK";
    case PROTOCOL_CMD_DICTIONARY_ACK: return "DICTIONARY_ACK";
    case PROTOCOL_CMD_CONFIG_COMMIT: return "CONFIG_COMMIT";
    case PROTOCOL_CMD_CONFIG_COMMIT_ACK: return "CONFIG_COMMIT_ACK";
    case PROTOCOL_CMD_ERROR: return "ERROR";
    default: return "UNKNOWN";
  }
}

static const char *Protocol_ErrorName(uint8_t reason)
{
  switch (reason)
  {
    case PROTOCOL_ERROR_INVALID_ADDR: return "INVALID_ADDR";
    case PROTOCOL_ERROR_ACCESS_DENIED: return "ACCESS_DENIED";
    case PROTOCOL_ERROR_OTHER: return "OTHER";
    default: return "UNKNOWN";
  }
}
