#include "ethernet.h"
#include "protocol.h"
#include "lwip/tcp.h"
#include "lwip/pbuf.h"
#include "lwip/mem.h"
#include <string.h>

typedef struct
{
    struct tcp_pcb *client;
    uint16_t rx_len;
    uint8_t  rx_buf[PROTOCOL_MAX_PAYLOAD + 5U];
} EthernetControlSession;

static struct tcp_pcb *g_listen_pcb;
static EthernetControlSession g_session;

static err_t Ethernet_OnAccept(void *arg, struct tcp_pcb *new_pcb, err_t err);
static err_t Ethernet_OnReceive(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static err_t Ethernet_OnSent(void *arg, struct tcp_pcb *tpcb, uint16_t len);
static err_t Ethernet_OnPoll(void *arg, struct tcp_pcb *tpcb);
static void Ethernet_OnError(void *arg, err_t err);
static void Ethernet_CloseClient(struct tcp_pcb *tpcb);
static void Ethernet_ProcessRxBuffer(void);
static void Ethernet_SendBytes(const uint8_t *data, uint16_t len);

void Ethernet_Init(void)
{
    memset(&g_session, 0, sizeof(g_session));
    Protocol_SetTxHandler(Ethernet_SendBytes);

    g_listen_pcb = tcp_new();
    if (g_listen_pcb == NULL)
    {
        return;
    }

    if (tcp_bind(g_listen_pcb, IP_ADDR_ANY, ETHERNET_CONTROL_TCP_PORT) != ERR_OK)
    {
        tcp_close(g_listen_pcb);
        g_listen_pcb = NULL;
        return;
    }

    g_listen_pcb = tcp_listen(g_listen_pcb);
    tcp_accept(g_listen_pcb, Ethernet_OnAccept);
}

void Ethernet_Process(void)
{
    /* lwIP callbacks handle all TCP processing. */
}

static err_t Ethernet_OnAccept(void *arg, struct tcp_pcb *new_pcb, err_t err)
{
    LWIP_UNUSED_ARG(arg);

    if (err != ERR_OK)
    {
        return err;
    }

    if (g_session.client != NULL)
    {
        Ethernet_CloseClient(g_session.client);
    }

    g_session.client = new_pcb;
    g_session.rx_len = 0U;

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
        return ERR_OK;
    }

    if (p == NULL)
    {
        Ethernet_CloseClient(tpcb);
        return ERR_OK;
    }

    tcp_recved(tpcb, p->tot_len); //acknoledge

    struct pbuf *q = p;
    while (q != NULL)
    {
        const uint8_t *chunk = (const uint8_t *)q->payload;
        uint16_t chunk_len = q->len;

        while (chunk_len > 0U)
        {
            uint16_t space = (uint16_t)(sizeof(session->rx_buf) - session->rx_len);
            uint16_t copy_len = (chunk_len < space) ? chunk_len : space;

            if (copy_len == 0U)
            {
                session->rx_len = 0U;
                space = (uint16_t)(sizeof(session->rx_buf));
                copy_len = (chunk_len < space) ? chunk_len : space;
            }

            memcpy(&session->rx_buf[session->rx_len], chunk, copy_len);
            session->rx_len = (uint16_t)(session->rx_len + copy_len);
            chunk = &chunk[copy_len];
            chunk_len = (uint16_t)(chunk_len - copy_len);

            Ethernet_ProcessRxBuffer();
        }

        q = q->next;
    }

    pbuf_free(p);
    return ERR_OK;
}

static err_t Ethernet_OnSent(void *arg, struct tcp_pcb *tpcb, uint16_t len)
{
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(tpcb);
    LWIP_UNUSED_ARG(len);
    return ERR_OK;
}

static err_t Ethernet_OnPoll(void *arg, struct tcp_pcb *tpcb)
{
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(tpcb);
    return ERR_OK;
}

static void Ethernet_OnError(void *arg, err_t err)
{
    EthernetControlSession *session = (EthernetControlSession *)arg;
    LWIP_UNUSED_ARG(err);

    if (session != NULL)
    {
        session->client = NULL;
        session->rx_len = 0U;
    }
}

static void Ethernet_CloseClient(struct tcp_pcb *tpcb)
{
    if (tpcb == NULL)
    {
        return;
    }

    tcp_arg(tpcb, NULL);
    tcp_recv(tpcb, NULL);
    tcp_sent(tpcb, NULL);
    tcp_err(tpcb, NULL);
    tcp_poll(tpcb, NULL, 0);

    (void)tcp_close(tpcb);

    if (g_session.client == tpcb)
    {
        g_session.client = NULL;
        g_session.rx_len = 0U;
    }
}

static void Ethernet_ProcessRxBuffer(void)
{
    while (g_session.rx_len >= 2U)
    {
        if (g_session.rx_buf[0] != PROTOCOL_START_BYTE)
        {
            memmove(g_session.rx_buf, &g_session.rx_buf[1], g_session.rx_len - 1U);
            g_session.rx_len--;
            continue;
        }

        if (g_session.rx_len < 2U)
        {
            return;
        }

        uint16_t frame_len = (uint16_t)(2U + g_session.rx_buf[1]);
        if (frame_len < 5U || frame_len > sizeof(g_session.rx_buf))
        {
            memmove(g_session.rx_buf, &g_session.rx_buf[1], g_session.rx_len - 1U);
            g_session.rx_len--;
            continue;
        }

        if (g_session.rx_len < frame_len)
        {
            return;
        }

        Protocol_ProcessBytes(g_session.rx_buf, frame_len);

        if (g_session.rx_len > frame_len)
        {
            memmove(g_session.rx_buf, &g_session.rx_buf[frame_len], g_session.rx_len - frame_len);
        }
        g_session.rx_len = (uint16_t)(g_session.rx_len - frame_len);
    }
}

static void Ethernet_SendBytes(const uint8_t *data, uint16_t len)
{
    if ((g_session.client == NULL) || (data == NULL) || (len == 0U))
    {
        return;
    }

    if (tcp_sndbuf(g_session.client) < len)
    {
        return;
    }

    if (tcp_write(g_session.client, data, len, TCP_WRITE_FLAG_COPY) == ERR_OK)
    {
        (void)tcp_output(g_session.client);
    }
}
