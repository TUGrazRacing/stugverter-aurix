#include "stream.h"
#include "parameter.h"
#include "controller.h"
#include "IfxCpu.h"
#include <string.h>

#define STREAM_ISR_LOCK_RETRIES 8U
#define STREAM_TX_FRAMES_PER_POLL 8U

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
static IfxCpu_mutexLock stream_lock;

static bool Stream_QueueFrame(uint8_t stream_id, uint16_t sequence, uint64_t timestamp_ticks, const uint8_t *payload, uint8_t payload_len);
static bool Stream_DequeueFrame(StreamFrame_t *frame);
static void Stream_PurgeFrames(uint8_t stream_id);
static bool Stream_TryLockFromIsr(void);
static void Stream_Lock(void);

void Stream_Init(void)
{
    memset(streams, 0, sizeof(streams));
    memset(frame_queue, 0, sizeof(frame_queue));
    frame_head = 0U;
    frame_tail = 0U;
    frame_count = 0U;
    stream_lock = 0U;
}

bool Stream_Start(uint8_t id, uint8_t loop_divider, const uint16_t *regs, uint8_t num_regs)
{
    StreamInfo_t next_stream;
    uint32_t total_payload = 0U;
    int free_idx = -1;

    if ((loop_divider == 0U) || (num_regs == 0U) || (regs == NULL) || (num_regs > STREAM_MAX_REGS))
    {
        return false;
    }

    memset(&next_stream, 0, sizeof(next_stream));

    for (uint8_t i = 0U; i < num_regs; i++)
    {
        uint8_t len = 0U;

        if (!getParameterLen(regs[i], &len) || (len == 0U))
        {
            return false;
        }

        if ((total_payload + len) > STREAM_FRAME_VALUE_BYTES)
        {
            return false;
        }

        next_stream.regs[i] = regs[i];
        next_stream.reg_lengths[i] = len;
        total_payload += len;
    }

    Stream_Lock();

    for (int i = 0; i < (int)STREAM_MAX_STREAMS; i++)
    {
        if (streams[i].active && (streams[i].id == id))
        {
            IfxCpu_releaseMutex(&stream_lock);
            return false;
        }

        if ((!streams[i].active) && (free_idx == -1))
        {
            free_idx = i;
        }
    }

    if (free_idx < 0)
    {
        IfxCpu_releaseMutex(&stream_lock);
        return false;
    }

    next_stream.active = true;
    next_stream.id = id;
    next_stream.sequence = 0U;
    next_stream.loop_divider = loop_divider;
    next_stream.next_loop_count = controllerGetLoopCounter() + (uint32_t)loop_divider;
    next_stream.num_regs = num_regs;
    streams[free_idx] = next_stream;

    IfxCpu_releaseMutex(&stream_lock);
    return true;
}

bool Stream_Stop(uint8_t id)
{
    bool stopped = false;

    Stream_Lock();

    for (int i = 0; i < (int)STREAM_MAX_STREAMS; i++)
    {
        if (streams[i].active && (streams[i].id == id))
        {
            streams[i].active = false;
            Stream_PurgeFrames(id);
            stopped = true;
            break;
        }
    }

    IfxCpu_releaseMutex(&stream_lock);
    return stopped;
}

void Stream_OnControlLoop(void)
{
    uint32_t current_loop_count = controllerGetLoopCounter();
    uint64_t current_ticks = controllerGetLastLoopTick();

    if (!Stream_TryLockFromIsr())
    {
        return;
    }

    for (int i = 0; i < (int)STREAM_MAX_STREAMS; i++)
    {
        if (streams[i].active && (current_loop_count >= streams[i].next_loop_count))
        {
            uint8_t payload[STREAM_FRAME_VALUE_BYTES];
            uint8_t payload_idx = 0U;
            bool valid = true;

            while (current_loop_count >= streams[i].next_loop_count)
            {
                streams[i].next_loop_count += (uint32_t)streams[i].loop_divider;
            }

            for (uint8_t r = 0U; r < streams[i].num_regs; r++)
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

    IfxCpu_releaseMutex(&stream_lock);
}

void Stream_TransmitPending(void)
{
    StreamFrame_t frame;
    uint8_t frames_sent = 0U;

    if (!Protocol_HasUdpSender())
    {
        return;
    }

    while ((frames_sent < STREAM_TX_FRAMES_PER_POLL) && Stream_DequeueFrame(&frame))
    {
        Protocol_SendStreamData(frame.stream_id, frame.sequence, frame.timestamp_ticks, frame.payload, frame.payload_len);
        frames_sent++;
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
    if (frame == NULL)
    {
        return false;
    }

    Stream_Lock();

    if (frame_count == 0U)
    {
        IfxCpu_releaseMutex(&stream_lock);
        return false;
    }

    *frame = frame_queue[frame_tail];
    frame_tail = (uint8_t)((frame_tail + 1U) % STREAM_FRAME_BUFFER_SIZE);
    frame_count--;
    IfxCpu_releaseMutex(&stream_lock);
    return true;
}

static bool Stream_TryLockFromIsr(void)
{
    for (uint8_t attempt = 0U; attempt < STREAM_ISR_LOCK_RETRIES; attempt++)
    {
        if (IfxCpu_acquireMutex(&stream_lock))
        {
            return true;
        }
    }

    return false;
}

static void Stream_Lock(void)
{
    while (!IfxCpu_acquireMutex(&stream_lock))
    {
        /* Wait for the short stream critical section to finish. */
    }
}

static void Stream_PurgeFrames(uint8_t stream_id)
{
    StreamFrame_t kept[STREAM_FRAME_BUFFER_SIZE];
    uint8_t kept_count = 0U;
    uint8_t read_idx = frame_tail;

    for (uint8_t i = 0U; i < frame_count; i++)
    {
        StreamFrame_t frame = frame_queue[read_idx];
        if (frame.stream_id != stream_id)
        {
            kept[kept_count++] = frame;
        }

        read_idx = (uint8_t)((read_idx + 1U) % STREAM_FRAME_BUFFER_SIZE);
    }

    frame_head = 0U;
    frame_tail = 0U;
    frame_count = 0U;

    for (uint8_t i = 0U; i < kept_count; i++)
    {
        frame_queue[frame_head] = kept[i];
        frame_head = (uint8_t)((frame_head + 1U) % STREAM_FRAME_BUFFER_SIZE);
        frame_count++;
    }
}
