#include "stream.h"
#include "protocol.h"
#include "parameter.h"
#include "IfxStm.h"
#include <string.h>

typedef struct
{
    bool     active;
    uint8_t  id;
    uint16_t sequence;
    uint64_t period_ticks;
    uint64_t next_tick;
    uint8_t  num_regs;
    uint8_t  reg_lengths[STREAM_MAX_REGS];
    uint16_t regs[STREAM_MAX_REGS];
} StreamInfo_t;

typedef struct
{
    uint8_t  stream_id;
    uint16_t sequence;
    uint64_t timestamp_ticks;
    uint8_t  register_count;
    uint8_t  payload_len;
    uint8_t  payload[STREAM_FRAME_VALUE_BYTES];
} StreamFrame_t;

static StreamInfo_t  streams[STREAM_MAX_STREAMS];
static StreamFrame_t frame_queue[STREAM_FRAME_BUFFER_SIZE];
static uint8_t       frame_head;
static uint8_t       frame_tail;
static uint8_t       frame_count;

static bool Stream_QueueFrame(uint8_t stream_id, uint16_t sequence, uint64_t timestamp_ticks, uint8_t register_count, const uint8_t *payload, uint8_t payload_len);
static bool Stream_DequeueFrame(StreamFrame_t *frame);
static void Stream_PurgeFrames(uint8_t stream_id);

void Stream_Init(void)
{
    memset(streams, 0, sizeof(streams));
    memset(frame_queue, 0, sizeof(frame_queue));
    frame_head = 0U;
    frame_tail = 0U;
    frame_count = 0U;
}

bool Stream_Start(uint8_t id, uint16_t freq_x100, const uint16_t *regs, uint8_t num_regs)
{
    if ((freq_x100 == 0U) || (num_regs == 0U) || (regs == NULL) || (num_regs > STREAM_MAX_REGS))
    {
        return false;
    }

    int free_idx = -1;
    for (int i = 0; i < (int)STREAM_MAX_STREAMS; i++)
    {
        if (streams[i].active && (streams[i].id == id))
        {
            return false;
        }
        if ((!streams[i].active) && (free_idx == -1))
        {
            free_idx = i;
        }
    }

    if (free_idx == -1)
    {
        return false;
    }

    memset(&streams[free_idx], 0, sizeof(StreamInfo_t));

    uint64_t timer_hz = (uint64_t)IfxStm_getFrequency(&MODULE_STM0);
    uint64_t period_ticks = (100ULL * timer_hz) / freq_x100;
    if (period_ticks == 0ULL)
    {
        return false;
    }

    uint32_t payload_budget = STREAM_FRAME_VALUE_BYTES;
    uint32_t total_payload = 0U;

    for (uint8_t i = 0; i < num_regs; i++)
    {
        uint8_t len = 0U;
        if (!getParameterLen(regs[i], &len))
        {
            return false;
        }
        total_payload += len;
        if (total_payload > payload_budget)
        {
            return false;
        }
        streams[free_idx].reg_lengths[i] = len;
    }

    streams[free_idx].active = true;
    streams[free_idx].id = id;
    streams[free_idx].sequence = 0U;
    streams[free_idx].period_ticks = period_ticks;
    streams[free_idx].next_tick = IfxStm_get(&MODULE_STM0) + period_ticks;
    streams[free_idx].num_regs = num_regs;
    memcpy(streams[free_idx].regs, regs, num_regs * sizeof(uint16_t));
    return true;
}

bool Stream_Stop(uint8_t id)
{
    for (int i = 0; i < (int)STREAM_MAX_STREAMS; i++)
    {
        if (streams[i].active && (streams[i].id == id))
        {
            streams[i].active = false;
            Stream_PurgeFrames(id);
            return true;
        }
    }
    return false;
}

void Stream_Process(void)
{
    uint64_t current_ticks = IfxStm_get(&MODULE_STM0);

    for (int i = 0; i < (int)STREAM_MAX_STREAMS; i++)
    {
        if (streams[i].active && (current_ticks >= streams[i].next_tick))
        {
            while (current_ticks >= streams[i].next_tick)
            {
                streams[i].next_tick += streams[i].period_ticks;
            }

            uint8_t payload[STREAM_FRAME_VALUE_BYTES];
            uint8_t payload_idx = 0U;
            bool valid = true;

            for (uint8_t r = 0; r < streams[i].num_regs; r++)
            {
                uint8_t value[8];
                uint8_t len = 0U;

                if (!readParameter(streams[i].regs[r], value, &len) || (len != streams[i].reg_lengths[r]))
                {
                    valid = false;
                    break;
                }

                if ((payload_idx + len) > sizeof(payload))
                {
                    valid = false;
                    break;
                }

                memcpy(&payload[payload_idx], value, len);
                payload_idx = (uint8_t)(payload_idx + len);
            }

            if (valid)
            {
                (void)Stream_QueueFrame(streams[i].id, streams[i].sequence++, current_ticks, streams[i].num_regs, payload, payload_idx);
            }
        }
    }
}

void Stream_TransmitPending(void)
{
    StreamFrame_t frame;

    if (!Protocol_HasUdpSender())
    {
        return;
    }

    while (Stream_DequeueFrame(&frame))
    {
        Protocol_SendStreamData(frame.stream_id, frame.sequence, frame.timestamp_ticks, frame.register_count, frame.payload, frame.payload_len);
    }
}

static bool Stream_QueueFrame(uint8_t stream_id, uint16_t sequence, uint64_t timestamp_ticks, uint8_t register_count, const uint8_t *payload, uint8_t payload_len)
{
    if ((payload == NULL) || (payload_len > sizeof(frame_queue[0].payload)))
    {
        return false;
    }

    if (frame_count == STREAM_FRAME_BUFFER_SIZE)
    {
        frame_tail = (uint8_t)((frame_tail + 1U) % STREAM_FRAME_BUFFER_SIZE);
        frame_count--;
    }

    frame_queue[frame_head].stream_id = stream_id;
    frame_queue[frame_head].sequence = sequence;
    frame_queue[frame_head].timestamp_ticks = timestamp_ticks;
    frame_queue[frame_head].register_count = register_count;
    frame_queue[frame_head].payload_len = payload_len;
    memcpy(frame_queue[frame_head].payload, payload, payload_len);

    frame_head = (uint8_t)((frame_head + 1U) % STREAM_FRAME_BUFFER_SIZE);
    frame_count++;
    return true;
}

static bool Stream_DequeueFrame(StreamFrame_t *frame)
{
    if ((frame == NULL) || (frame_count == 0U))
    {
        return false;
    }

    *frame = frame_queue[frame_tail];
    frame_tail = (uint8_t)((frame_tail + 1U) % STREAM_FRAME_BUFFER_SIZE);
    frame_count--;
    return true;
}

static void Stream_PurgeFrames(uint8_t stream_id)
{
    StreamFrame_t kept[STREAM_FRAME_BUFFER_SIZE];
    uint8_t kept_count = 0U;

    while (frame_count > 0U)
    {
        StreamFrame_t frame;
        (void)Stream_DequeueFrame(&frame);
        if (frame.stream_id != stream_id)
        {
            kept[kept_count++] = frame;
        }
    }

    frame_head = 0U;
    frame_tail = 0U;
    frame_count = 0U;

    for (uint8_t i = 0U; i < kept_count; i++)
    {
        (void)Stream_QueueFrame(kept[i].stream_id, kept[i].sequence, kept[i].timestamp_ticks, kept[i].register_count, kept[i].payload, kept[i].payload_len);
    }
}
