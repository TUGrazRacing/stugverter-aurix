#include "stream.h"
#include "protocol.h"
#include "parameter.h"
#include "IfxStm.h"
#include <string.h>

typedef struct {
    bool     active;
    uint8_t  id;
    uint64_t period_ticks;
    uint64_t next_tick;
    uint8_t  num_regs;
    uint16_t regs[STREAM_MAX_REGS];
} StreamInfo_t;

static StreamInfo_t streams[STREAM_MAX_STREAMS];

void Stream_Init(void)
{
    memset(streams, 0, sizeof(streams));
}

bool Stream_Start(uint8_t id, uint16_t freq_x100, const uint16_t *regs, uint8_t num_regs)
{
    if (freq_x100 == 0 || num_regs == 0 || regs == NULL) {
        return false;
    }

    int free_idx = -1;

    /* Check if ID is already taken, and find a free slot */
    for (int i = 0; i < STREAM_MAX_STREAMS; i++) {
        if (streams[i].active && streams[i].id == id) {
            return false; // ID already taken
        }
        if (!streams[i].active && free_idx == -1) {
            free_idx = i;
        }
    }

    if (free_idx == -1) {
        return false; // No available stream slots
    }

    /* Calculate timer ticks per cycle.
     * frequency = freq_x100 / 100.0 (Hz)
     * period_sec = 1 / frequency = 100.0 / freq_x100
     * ticks = (100 * timer_hz) / freq_x100
     */
    uint64_t timer_hz = IfxStm_getFrequency(&MODULE_STM0);
    uint64_t period_ticks = (100ULL * timer_hz) / freq_x100;

    /* Initialize the stream */
    streams[free_idx].id = id;
    streams[free_idx].period_ticks = period_ticks;
    streams[free_idx].next_tick = IfxStm_get(&MODULE_STM0) + period_ticks;
    streams[free_idx].num_regs = num_regs;
    memcpy(streams[free_idx].regs, regs, num_regs * sizeof(uint16_t));

    streams[free_idx].active = true;

    return true;
}

bool Stream_Stop(uint8_t id)
{
    for (int i = 0; i < STREAM_MAX_STREAMS; i++) {
        if (streams[i].active && streams[i].id == id) {
            streams[i].active = false;
            return true;
        }
    }
    return false; // ID not found
}

void Stream_Process(void)
{
    uint64_t current_ticks = IfxStm_get(&MODULE_STM0);

    for (int i = 0; i < STREAM_MAX_STREAMS; i++)
    {
        if (streams[i].active && (current_ticks >= streams[i].next_tick))
        {
            /* Schedule next transmission */
            streams[i].next_tick = current_ticks + streams[i].period_ticks;

            uint8_t payload[PROTOCOL_MAX_PAYLOAD];
            uint8_t payload_idx = 0;

            /* Gather register data */
            for (uint8_t r = 0; r < streams[i].num_regs; r++) {
                uint8_t val[8]; // Generous local buffer for a single parameter
                uint8_t len = 0;

                if (readParameter(streams[i].regs[r], val, &len)) {
                    /* Protect against payload buffer overflow */
                    if ((payload_idx + len) <= PROTOCOL_MAX_PAYLOAD) {
                        memcpy(&payload[payload_idx], val, len);
                        payload_idx += len;
                    }
                }
            }

            /* Send out the assembled data packet over UART */
            if (payload_idx > 0) {
                Protocol_SendStreamData(streams[i].id, payload, payload_idx);
            }
        }
    }
}
