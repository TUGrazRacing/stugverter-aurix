#include "stream.h"
#include "parameter.h"
#include "motor_control.h"
#include <string.h>

typedef struct
{
    bool active;
    uint8_t id;
    uint16_t sequence;
    uint8_t  loop_divider;
    uint32_t next_loop_count;
    uint8_t  num_regs;
    uint8_t  reg_lengths[STREAM_MAX_REGS];
    uint16_t regs[STREAM_MAX_REGS];
} StreamInfo_t;

typedef struct
{
    uint8_t stream_id;
    uint16_t sequence;
    uint64_t timestamp_ticks;
    uint8_t  payload_len;
    uint8_t  payload[STREAM_FRAME_VALUE_BYTES];
} StreamFrame_t;

static StreamInfo_t streams[STREAM_MAX_STREAMS];
static StreamFrame_t frame_queue[STREAM_FRAME_BUFFER_SIZE];
static uint8_t frame_head;
static uint8_t frame_tail;
static uint8_t frame_count;

static bool Stream_QueueFrame(uint8_t stream_id, uint16_t sequence, uint64_t timestamp_ticks, const uint8_t *payload, uint8_t payload_len);
static bool Stream_DequeueFrame(StreamFrame_t *frame);
static bool Stream_PeekFrame(StreamFrame_t *frame);
static void Stream_PurgeFrames(uint8_t stream_id);
static void Stream_CaptureStream(StreamInfo_t *stream);

void Stream_Init(void)
{
    memset(streams, 0, sizeof(streams));
    memset(frame_queue, 0, sizeof(frame_queue));
    frame_head = 0U;
    frame_tail = 0U;
    frame_count = 0U;
}

bool Stream_Start(uint8_t id, uint8_t loop_divider, const uint16_t *regs, uint8_t num_regs)
{
    if ((loop_divider == 0U) || (num_regs == 0U) || (regs == NULL) || (num_regs > STREAM_MAX_REGS))
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

    uint32_t payload_budget = STREAM_FRAME_VALUE_BYTES;
    uint32_t total_payload = 0U;

    for (uint8_t i = 0; i < num_regs; i++)
    {
        const void *read_ptr = NULL;
        uint8_t len = 0U;

        if (!getParameterPointer(regs[i], &read_ptr, &len))
        {
            return false;
        }

        if ((uint16_t)(total_payload + len) > STREAM_FRAME_VALUE_BYTES)
        {
            return false;
        }

        streams[free_idx].reg_ptrs[i] = (const uint8_t *)read_ptr;
        streams[free_idx].reg_lengths[i] = len;
        total_payload = (uint8_t)(total_payload + len);
    }

    streams[free_idx].active = true;
    streams[free_idx].id = id;
    streams[free_idx].sequence = 0U;
    streams[free_idx].loop_divider = loop_divider;
    streams[free_idx].next_loop_count = controllerGetLoopCounter() + (uint32_t)loop_divider;
    streams[free_idx].num_regs = num_regs;

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

void Stream_OnControlLoop(void)
{
    uint32_t current_loop_count = controllerGetLoopCounter();
    uint64_t current_ticks = controllerGetLastLoopTick();

    for (int i = 0; i < (int)STREAM_MAX_STREAMS; i++)
    {
        if (streams[i].active && (current_loop_count >= streams[i].next_loop_count))
        {
            while (current_loop_count >= streams[i].next_loop_count)
            {
                streams[i].next_loop_count += (uint32_t)streams[i].loop_divider;

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
                    (void)Stream_QueueFrame(streams[i].id, streams[i].sequence++, current_ticks, payload, payload_idx);
                }
            }
        }

        if (stream->loop_counter > 1U)
        {
            stream->loop_counter--;
            continue;
        }

        stream->loop_counter = stream->loop_divider;
        Stream_CaptureStream(stream);
    }
}

void Stream_TransmitPending(void)
{
    if (!Protocol_HasUdpSender())
    {
        return;
    }

    while (frame_count > 0U)
    {
        Protocol_SendStreamData(frame.stream_id, frame.sequence, frame.timestamp_ticks, frame.payload, frame.payload_len);
    }
}

static bool Stream_QueueFrame(uint8_t stream_id, uint16_t sequence, uint64_t timestamp_ticks, const uint8_t *payload, uint8_t payload_len)
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

static bool Stream_PeekFrame(StreamFrame_t *frame)
{
    if ((frame == NULL) || (frame_count == 0U))
    {
        return false;
    }

    *frame = frame_queue[frame_tail];
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
        (void)Stream_QueueFrame(kept[i].stream_id, kept[i].sequence, kept[i].timestamp_ticks, kept[i].payload, kept[i].payload_len);
    }
}

static void Stream_CaptureStream(StreamInfo_t *stream)
{
    StreamFrame_t *slot;
    uint8_t payload_idx = 0U;

    if (stream == NULL)
    {
        return;
    }

    for (uint8_t r = 0U; r < stream->num_regs; r++)
    {
        uint8_t len = stream->reg_lengths[r];

        if ((payload_idx + len) > sizeof(frame_queue[0].payload))
        {
            return;
        }

        payload_idx = (uint8_t)(payload_idx + len);
    }

    if (frame_count == STREAM_FRAME_BUFFER_SIZE)
    {
        frame_tail = (uint8_t)((frame_tail + 1U) % STREAM_FRAME_BUFFER_SIZE);
        frame_count--;
    }

    slot = &frame_queue[frame_head];
    slot->stream_id = stream->id;
    slot->sequence = stream->sequence++;
    slot->timestamp_ticks = IfxStm_get(IFXSTM_DEFAULT_TIMER);
    slot->register_count = stream->num_regs;
    slot->payload_len = payload_idx;
    for (uint8_t r = 0U, offset = 0U; r < stream->num_regs; r++)
    {
        uint8_t len = stream->reg_lengths[r];
        memcpy(&slot->payload[offset], stream->reg_ptrs[r], len);
        offset = (uint8_t)(offset + len);
    }
    frame_head = (uint8_t)((frame_head + 1U) % STREAM_FRAME_BUFFER_SIZE);
    frame_count++;
}
