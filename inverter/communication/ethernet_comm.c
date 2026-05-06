#include <ethernet_comm.h>
#include "Configuration.h"
#include "IfxCpu.h"
#include "Ifx_Lwip.h"
#include "IfxStm.h"
#include "UART_Logging.h"
#include "lwip/dhcp.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "protocol.h"
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define ETHERNET_DEBUG_ENABLE 1
#define ETHERNET_LOG_BUFFER_SIZE 160U

/* Ring buffer for TCP RX stream bytes (no memmove hot path). */
#define ETHERNET_RX_RING_SIZE ((PROTOCOL_MAX_PAYLOAD + PROTOCOL_TCP_HEADER_SIZE) * 4U)

/* Queue for TCP TX frames when lwIP send buffer is temporarily full. */
#define ETHERNET_TX_QUEUE_DEPTH 16U
#define ETHERNET_TX_FRAME_MAX (PROTOCOL_MAX_PAYLOAD + PROTOCOL_TCP_HEADER_SIZE)

typedef struct
{
    uint16_t len;
    uint8_t data[ETHERNET_TX_FRAME_MAX];
} EthernetTxFrame;

typedef struct
{
    struct tcp_pcb *client;

    uint8_t rx_ring[ETHERNET_RX_RING_SIZE];
    uint16_t rx_head;
    uint16_t rx_tail;
    uint16_t rx_count;

    EthernetTxFrame tx_queue[ETHERNET_TX_QUEUE_DEPTH];
    uint8_t tx_head;
    uint8_t tx_tail;
    uint8_t tx_count;
} EthernetControlSession;


typedef struct
{
    bool initialized;
    bool linkUp;
    bool dhcpRunning;
    uint32 lastIpAddr;
} EthernetRuntimeState;
static struct tcp_pcb *g_listen_pcb;
static EthernetControlSession g_session;

static EthernetRuntimeState g_state;

static void Ethernet_ConfigureTimer(void);
static void Ethernet_ControlServerInit(void);
static void Ethernet_ApplyStaticAddress(struct netif *netif);
static void Ethernet_UpdateNetworkState(void);
static void Ethernet_LogNetifIp(const char *prefix, struct netif *netif);
static err_t Ethernet_OnAccept(void *arg, struct tcp_pcb *new_pcb, err_t err);
static err_t Ethernet_OnReceive(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static err_t Ethernet_OnSent(void *arg, struct tcp_pcb *tpcb, uint16_t len);
static err_t Ethernet_OnPoll(void *arg, struct tcp_pcb *tpcb);
static void Ethernet_OnError(void *arg, err_t err);
static void Ethernet_CloseClient(struct tcp_pcb *tpcb);

static void Ethernet_ProcessRxRing(void);
static bool Ethernet_RxPushByte(uint8_t b);
static bool Ethernet_RxPeek(uint16_t offset, uint8_t *out);
static void Ethernet_RxDrop(uint16_t count);
static void Ethernet_RxCopyOut(uint8_t *dst, uint16_t count);

static void Ethernet_SendBytes(const uint8_t *data, uint16_t len);
static bool Ethernet_TxEnqueue(const uint8_t *data, uint16_t len);
static void Ethernet_TxFlush(void);
static void Ethernet_TxReset(void);

static void Ethernet_Log(const char *fmt, ...);

IFX_INTERRUPT(updateLwIPStackISR, CPU_WHICH_SERVICE_ETHERNET, ISR_PRIORITY_OS_TICK)
{
    IfxStm_increaseCompare(&MODULE_STM0, IfxStm_Comparator_0, IFX_CFG_STM_TICKS_PER_MS);
    g_TickCount_1ms++;
    Ifx_Lwip_onTimerTick();
}

void Ethernet_Init(void)
{
    eth_addr_t ethAddr;

    memset(&g_session, 0, sizeof(g_session));
    memset(&g_state, 0, sizeof(g_state));

    Ethernet_ConfigureTimer();

    IfxGeth_enableModule(&MODULE_GETH);
    Ethernet_Log("ETH: GETH module enabled\r\n");

    MAC_ADDR(&ethAddr, 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED);
    Ifx_Lwip_init(ethAddr);
    Ethernet_Log("ETH: lwIP initialized\r\n");

    Ethernet_ControlServerInit();

    if (ETHERNET_USE_DHCP == 0U)
    {
        Ethernet_ApplyStaticAddress(Ifx_Lwip_getNetIf());
    }
    else
    {
        g_state.dhcpRunning = true;
    }

    g_state.initialized = true;
}

void Ethernet_Process(void)
{
    if (!g_state.initialized)
    {
        return;
    }

    Ifx_Lwip_pollTimerFlags();
    Ifx_Lwip_pollReceiveFlags();
    Ethernet_UpdateNetworkState();
    Ethernet_TxFlush();
    Stream_TransmitPending();
}

static void Ethernet_ConfigureTimer(void)
{
    IfxStm_CompareConfig stmCompareConfig;

    IfxStm_initCompareConfig(&stmCompareConfig);
    stmCompareConfig.triggerPriority     = ISR_PRIORITY_OS_TICK;
    stmCompareConfig.comparatorInterrupt = IfxStm_ComparatorInterrupt_ir0;
    stmCompareConfig.ticks               = IFX_CFG_STM_TICKS_PER_MS * 10U;
    stmCompareConfig.typeOfService       = IfxSrc_Tos_cpu2;

    IfxStm_initCompare(&MODULE_STM0, &stmCompareConfig);
}

static void Ethernet_ControlServerInit(void)
{
    Protocol_Init();
    Protocol_NetworkInit();
    Protocol_SetTxHandler(Ethernet_SendBytes);

    Ethernet_Log("ETH: init TCP control server port=%u\r\n", (unsigned int)ETHERNET_CONTROL_TCP_PORT);

    g_listen_pcb = tcp_new();
    if (g_listen_pcb == NULL)
    {
        Ethernet_Log("ETH: tcp_new failed\r\n");
        return;
    }

    if (tcp_bind(g_listen_pcb, IP_ADDR_ANY, ETHERNET_CONTROL_TCP_PORT) != ERR_OK)
    {
        Ethernet_Log("ETH: tcp_bind failed\r\n");
        tcp_close(g_listen_pcb);
        g_listen_pcb = NULL;
        return;
    }

    g_listen_pcb = tcp_listen(g_listen_pcb);
    tcp_accept(g_listen_pcb, Ethernet_OnAccept);
    Ethernet_Log("ETH: TCP control server listening\r\n");
}

static void Ethernet_ApplyStaticAddress(struct netif *netif)
{
    ip4_addr_t ipAddr;
    ip4_addr_t netmask;
    ip4_addr_t gateway;

    if (netif == NULL)
    {
        return;
    }

    IP4_ADDR(&ipAddr,
             ETHERNET_STATIC_IP_0,
             ETHERNET_STATIC_IP_1,
             ETHERNET_STATIC_IP_2,
             ETHERNET_STATIC_IP_3);
    IP4_ADDR(&netmask,
             ETHERNET_STATIC_NETMASK_0,
             ETHERNET_STATIC_NETMASK_1,
             ETHERNET_STATIC_NETMASK_2,
             ETHERNET_STATIC_NETMASK_3);
    IP4_ADDR(&gateway,
             ETHERNET_STATIC_GATEWAY_0,
             ETHERNET_STATIC_GATEWAY_1,
             ETHERNET_STATIC_GATEWAY_2,
             ETHERNET_STATIC_GATEWAY_3);

    dhcp_release_and_stop(netif);
    netif_set_addr(netif, &ipAddr, &netmask, &gateway);
    g_state.lastIpAddr = ip4_addr_get_u32(&ipAddr);

    Ethernet_LogNetifIp("ETH: static address applied", netif);
}

static void Ethernet_UpdateNetworkState(void)
{
    struct netif *netif = Ifx_Lwip_getNetIf();
    bool linkUp = netif_is_link_up(netif) ? true : false;

    if (linkUp != g_state.linkUp)
    {
        g_state.linkUp = linkUp;

        if (g_state.linkUp)
        {
            Ethernet_Log("ETH: link up\r\n");

#if ETHERNET_USE_DHCP
            if (!g_state.dhcpRunning)
            {
                err_t dhcpErr = dhcp_start(netif);
                if (dhcpErr == ERR_OK)
                {
                    g_state.dhcpRunning = true;
                    Ethernet_Log("ETH: DHCP started\r\n");
                }
                else
                {
                    Ethernet_Log("ETH: DHCP start failed err=%d\r\n", (int)dhcpErr);
                }
            }
#else
            Ethernet_ApplyStaticAddress(netif);
#endif
        }
        else
        {
            Ethernet_Log("ETH: link down\r\n");

#if ETHERNET_USE_DHCP
            if (g_state.dhcpRunning)
            {
                dhcp_release_and_stop(netif);
                g_state.dhcpRunning = false;
                Ethernet_Log("ETH: DHCP stopped\r\n");
            }

            ip4_addr_t zeroIp;
            ip4_addr_set_zero(&zeroIp);
            netif_set_addr(netif, &zeroIp, &zeroIp, &zeroIp);
#endif
            g_state.lastIpAddr = 0U;
        }
    }

    if (linkUp)
    {
        const ip4_addr_t *ip = netif_ip4_addr(netif);
        uint32 currentIpAddr = ip4_addr_get_u32(ip);

        if ((currentIpAddr != 0U) && (currentIpAddr != g_state.lastIpAddr))
        {
            g_state.lastIpAddr = currentIpAddr;
            Ethernet_LogNetifIp((ETHERNET_USE_DHCP != 0U) ? "ETH: DHCP address assigned" : "ETH: static address active", netif);
        }
    }
}

static void Ethernet_LogNetifIp(const char *prefix, struct netif *netif)
{
    char msg[160];

    const ip4_addr_t *ip   = netif_ip4_addr(netif);
    const ip4_addr_t *mask = netif_ip4_netmask(netif);
    const ip4_addr_t *gw   = netif_ip4_gw(netif);

    int len = snprintf(msg, sizeof(msg),
        "%s IP=%u.%u.%u.%u MASK=%u.%u.%u.%u GW=%u.%u.%u.%u\r\n",
        prefix,
        (unsigned int)ip4_addr1(ip),   (unsigned int)ip4_addr2(ip),
        (unsigned int)ip4_addr3(ip),   (unsigned int)ip4_addr4(ip),
        (unsigned int)ip4_addr1(mask), (unsigned int)ip4_addr2(mask),
        (unsigned int)ip4_addr3(mask), (unsigned int)ip4_addr4(mask),
        (unsigned int)ip4_addr1(gw),   (unsigned int)ip4_addr2(gw),
        (unsigned int)ip4_addr3(gw),   (unsigned int)ip4_addr4(gw));

    if (len > 0)
    {
        if ((size_t)len >= sizeof(msg))
        {
            len = (int)(sizeof(msg) - 1U);
        }

        sendUARTMessage(msg, (Ifx_SizeT)len);
    }
}

static err_t Ethernet_OnAccept(void *arg, struct tcp_pcb *new_pcb, err_t err)
{
    LWIP_UNUSED_ARG(arg);

    if (err != ERR_OK)
    {
        Ethernet_Log("ETH: accept error=%d\r\n", (int)err);
        return err;
    }

    if (g_session.client != NULL)
    {
        Ethernet_Log("ETH: closing previous client\r\n");
        Ethernet_CloseClient(g_session.client);
    }

    g_session.client = new_pcb;
    g_session.rx_head = 0U;
    g_session.rx_tail = 0U;
    g_session.rx_count = 0U;
    Ethernet_TxReset();

    Ethernet_Log("ETH: client accepted\r\n");

    tcp_arg(new_pcb, &g_session);
    tcp_recv(new_pcb, Ethernet_OnReceive);
    tcp_sent(new_pcb, Ethernet_OnSent);
    tcp_err(new_pcb, Ethernet_OnError);
    tcp_poll(new_pcb, Ethernet_OnPoll, 2);

    return ERR_OK;
}

static err_t Ethernet_OnReceive(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    EthernetControlSession *session = (EthernetControlSession *)arg;

    if ((session == NULL) || (err != ERR_OK))
    {
        if (p != NULL)
        {
            pbuf_free(p);
        }
        Ethernet_Log("ETH: receive callback error=%d\r\n", (int)err);
        return ERR_OK;
    }

    if (p == NULL)
    {
        Ethernet_Log("ETH: client disconnected\r\n");
        Ethernet_CloseClient(tpcb);
        return ERR_OK;
    }

    tcp_recved(tpcb, p->tot_len);
    Ethernet_Log("ETH: rx pbuf tot_len=%u\r\n", (unsigned int)p->tot_len);

    for (struct pbuf *q = p; q != NULL; q = q->next)
    {
        const uint8_t *chunk = (const uint8_t *)q->payload;
        uint16_t chunk_len = q->len;
        Ethernet_Log("ETH: rx chunk len=%u\r\n", (unsigned int)chunk_len);

        for (uint16_t i = 0U; i < chunk_len; i++)
        {
            if (!Ethernet_RxPushByte(chunk[i]))
            {
                /* Keep newest bytes by discarding oldest byte on overflow. */
                Ethernet_RxDrop(1U);
                (void)Ethernet_RxPushByte(chunk[i]);
            }
        }
    }

    pbuf_free(p);

    /* Parse as many complete protocol frames as possible. */
    Ethernet_ProcessRxRing();
    return ERR_OK;
}

static err_t Ethernet_OnSent(void *arg, struct tcp_pcb *tpcb, uint16_t len)
{
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(tpcb);
    Ethernet_Log("ETH: tx complete len=%u\r\n", (unsigned int)len);
    Ethernet_TxFlush();
    return ERR_OK;
}

static err_t Ethernet_OnPoll(void *arg, struct tcp_pcb *tpcb)
{
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(tpcb);
    Ethernet_TxFlush();
    return ERR_OK;
}

static void Ethernet_OnError(void *arg, err_t err)
{
    EthernetControlSession *session = (EthernetControlSession *)arg;

    if (session != NULL)
    {
        session->client = NULL;
        session->rx_head = 0U;
        session->rx_tail = 0U;
        session->rx_count = 0U;
        Ethernet_TxReset();
    }

    Ethernet_Log("ETH: fatal tcp error=%d\r\n", (int)err);
}

static void Ethernet_CloseClient(struct tcp_pcb *tpcb)
{
    if (tpcb == NULL)
    {
        return;
    }

    Ethernet_Log("ETH: close client\r\n");

    tcp_arg(tpcb, NULL);
    tcp_recv(tpcb, NULL);
    tcp_sent(tpcb, NULL);
    tcp_err(tpcb, NULL);
    tcp_poll(tpcb, NULL, 0);
    (void)tcp_close(tpcb);

    if (g_session.client == tpcb)
    {
        g_session.client = NULL;
        g_session.rx_head = 0U;
        g_session.rx_tail = 0U;
        g_session.rx_count = 0U;
        Ethernet_TxReset();
    }
}

static void Ethernet_ProcessRxRing(void)
{
    uint8_t b0;
    uint8_t b1;

    while (g_session.rx_count >= PROTOCOL_TCP_HEADER_SIZE)
    {
        /* Search start marker AA55. */
        (void)Ethernet_RxPeek(0U, &b0);
        (void)Ethernet_RxPeek(1U, &b1);
        if ((b0 != PROTOCOL_START_BYTE_0) || (b1 != PROTOCOL_START_BYTE_1))
        {
            Ethernet_Log("ETH: resync drop byte 0x%02X\r\n", (unsigned int)b0);
            Ethernet_RxDrop(1U);
            continue;
        }

        uint8_t len_lo;
        uint8_t len_hi;
        (void)Ethernet_RxPeek(3U, &len_lo);
        (void)Ethernet_RxPeek(4U, &len_hi);

        uint16_t payload_len = (uint16_t)len_lo | ((uint16_t)len_hi << 8);
        uint16_t frame_len = (uint16_t)(PROTOCOL_TCP_HEADER_SIZE + payload_len);

        if (payload_len > PROTOCOL_MAX_PAYLOAD)
        {
            Ethernet_Log("ETH: invalid payload len=%u\r\n", (unsigned int)payload_len);
            Ethernet_RxDrop(1U);
            continue;
        }

        if (g_session.rx_count < frame_len)
        {
            Ethernet_Log("ETH: waiting frame len=%u have=%u\r\n", (unsigned int)frame_len, (unsigned int)g_session.rx_count);
            return;
        }

        uint8_t frame[ETHERNET_TX_FRAME_MAX];
        Ethernet_RxCopyOut(frame, frame_len);
        Ethernet_RxDrop(frame_len);

        Ethernet_Log("ETH: frame complete len=%u\r\n", (unsigned int)frame_len);
        Protocol_ProcessBytes(frame, frame_len);
    }
}

static bool Ethernet_RxPushByte(uint8_t b)
{
    if (g_session.rx_count >= ETHERNET_RX_RING_SIZE)
    {
        Ethernet_Log("ETH: rx ring overflow\r\n");
        return false;
    }

    g_session.rx_ring[g_session.rx_head] = b;
    g_session.rx_head = (uint16_t)((g_session.rx_head + 1U) % ETHERNET_RX_RING_SIZE);
    g_session.rx_count++;
    return true;
}

static bool Ethernet_RxPeek(uint16_t offset, uint8_t *out)
{
    if ((out == NULL) || (offset >= g_session.rx_count))
    {
        return false;
    }

    uint16_t idx = (uint16_t)((g_session.rx_tail + offset) % ETHERNET_RX_RING_SIZE);
    *out = g_session.rx_ring[idx];
    return true;
}

static void Ethernet_RxDrop(uint16_t count)
{
    if (count > g_session.rx_count)
    {
        count = g_session.rx_count;
    }

    g_session.rx_tail = (uint16_t)((g_session.rx_tail + count) % ETHERNET_RX_RING_SIZE);
    g_session.rx_count = (uint16_t)(g_session.rx_count - count);
}

static void Ethernet_RxCopyOut(uint8_t *dst, uint16_t count)
{
    if ((dst == NULL) || (count > g_session.rx_count))
    {
        return;
    }

    for (uint16_t i = 0U; i < count; i++)
    {
        (void)Ethernet_RxPeek(i, &dst[i]);
    }
}

static void Ethernet_SendBytes(const uint8_t *data, uint16_t len)
{
    if ((g_session.client == NULL) || (data == NULL) || (len == 0U))
    {
        Ethernet_Log("ETH: tx dropped client=%s len=%u\r\n", (g_session.client == NULL) ? "none" : "set", (unsigned int)len);
        return;
    }

    if (!Ethernet_TxEnqueue(data, len))
    {
        Ethernet_Log("ETH: tx queue overflow len=%u\r\n", (unsigned int)len);
        return;
    }

    Ethernet_TxFlush();
}

static bool Ethernet_TxEnqueue(const uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len == 0U) || (len > ETHERNET_TX_FRAME_MAX))
    {
        return false;
    }

    if (g_session.tx_count >= ETHERNET_TX_QUEUE_DEPTH)
    {
        return false;
    }

    EthernetTxFrame *slot = &g_session.tx_queue[g_session.tx_head];
    slot->len = len;
    memcpy(slot->data, data, len);

    g_session.tx_head = (uint8_t)((g_session.tx_head + 1U) % ETHERNET_TX_QUEUE_DEPTH);
    g_session.tx_count++;
    return true;
}

static void Ethernet_TxFlush(void)
{
    bool wrote = false;

    if (g_session.client == NULL)
    {
        return;
    }

    while (g_session.tx_count > 0U)
    {
        EthernetTxFrame *frame = &g_session.tx_queue[g_session.tx_tail];

        if (tcp_sndbuf(g_session.client) < frame->len)
        {
            Ethernet_Log("ETH: tx backlog need=%u have=%u queued=%u\r\n",
                         (unsigned int)frame->len,
                         (unsigned int)tcp_sndbuf(g_session.client),
                         (unsigned int)g_session.tx_count);
            break;
        }

        err_t err = tcp_write(g_session.client, frame->data, frame->len, TCP_WRITE_FLAG_COPY);
        if (err != ERR_OK)
        {
            Ethernet_Log("ETH: tcp_write failed err=%d len=%u\r\n", (int)err, (unsigned int)frame->len);
            break;
        }

        g_session.tx_tail = (uint8_t)((g_session.tx_tail + 1U) % ETHERNET_TX_QUEUE_DEPTH);
        g_session.tx_count--;
        wrote = true;

        Ethernet_Log("ETH: tx queued len=%u pending=%u\r\n",
                     (unsigned int)frame->len,
                     (unsigned int)g_session.tx_count);
    }

    if (wrote)
    {
        (void)tcp_output(g_session.client);
    }
}

static void Ethernet_TxReset(void)
{
    g_session.tx_head = 0U;
    g_session.tx_tail = 0U;
    g_session.tx_count = 0U;
}

static void Ethernet_Log(const char *fmt, ...)
{
#if ETHERNET_DEBUG_ENABLE
    char buffer[ETHERNET_LOG_BUFFER_SIZE];
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
