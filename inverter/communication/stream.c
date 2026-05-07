#include "stream.h"
#include "parameter.h"
#include "controller.h"
#include "IfxCpu.h"
#include <string.h>

#define STREAM_ISR_LOCK_RETRIES 8U
#define STREAM_TX_PACKETS_PER_POLL 8U
#define STREAM_BATCH_TARGET_LATENCY_LOOPS 100U

typedef struct
{
    uint8_t stream_id;
    uint8_t sample_count;
    uint16_t payload_len;
    uint8_t payload[STREAM_PACKET_PAYLOAD_BYTES];
} StreamPacket_t;

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
    uint8_t  batch_sample_count;
    uint8_t  batch_target_samples;
    uint32_t batch_max_age_loops;
    uint16_t batch_payload_len;
    uint32_t batch_first_loop_count;
    uint8_t  batch_payload[STREAM_PACKET_PAYLOAD_BYTES];
} StreamInfo_t;

typedef struct
{
    uint16_t sequence;
    uint64_t timestamp_ticks;
    uint8_t  payload_len;
    uint8_t  payload[STREAM_FRAME_VALUE_BYTES];
} StreamFrame_t;

static StreamInfo_t streams[STREAM_MAX_STREAMS];
static StreamPacket_t packet_queue[STREAM_PACKET_BUFFER_SIZE];
static uint8_t packet_head;
static uint8_t packet_tail;
static uint8_t packet_count;
static IfxCpu_mutexLock stream_lock;

static bool Stream_QueuePacket(const StreamPacket_t *packet);
static bool Stream_DequeuePacket(StreamPacket_t *packet);
static void Stream_PurgePackets(uint8_t stream_id);
static bool Stream_TryLockFromIsr(void);
static void Stream_Lock(void);
static void Stream_AppendFrame(StreamInfo_t *stream, const StreamFrame_t *frame, uint32_t current_loop_count);
static void Stream_FlushBatch(StreamInfo_t *stream);
static void Stream_FlushStaleBatches(uint32_t current_loop_count);
static uint8_t Stream_MaxSamplesPerPacket(uint8_t payload_len);
static uint8_t Stream_TargetSamplesPerPacket(uint8_t loop_divider, uint8_t payload_len);

void Stream_Init(void)
{
    memset(streams, 0, sizeof(streams));
    memset(packet_queue, 0, sizeof(packet_queue));
    packet_head = 0U;
    packet_tail = 0U;
    packet_count = 0U;
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
    next_stream.batch_target_samples = Stream_TargetSamplesPerPacket(loop_divider, (uint8_t)total_payload);
    next_stream.batch_max_age_loops = (uint32_t)loop_divider * (uint32_t)next_stream.batch_target_samples;
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
            Stream_FlushBatch(&streams[i]);
            streams[i].active = false;
            Stream_PurgePackets(id);
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
                StreamFrame_t frame;
                frame.sequence = streams[i].sequence++;
                frame.timestamp_ticks = current_ticks;
                frame.payload_len = payload_idx;
                memcpy(frame.payload, payload, payload_idx);
                Stream_AppendFrame(&streams[i], &frame, current_loop_count);
            }
        }

        if (streams[i].active && (streams[i].batch_sample_count > 0U) &&
            ((uint32_t)(current_loop_count - streams[i].batch_first_loop_count) >= streams[i].batch_max_age_loops))
        {
            Stream_FlushBatch(&streams[i]);
        }
    }

    IfxCpu_releaseMutex(&stream_lock);
}

void Stream_TransmitPending(void)
{
    StreamPacket_t packet;
    uint8_t packets_sent = 0U;

    if (!Protocol_HasUdpSender())
    {
        return;
    }

    Stream_Lock();
    Stream_FlushStaleBatches(controllerGetLoopCounter());
    IfxCpu_releaseMutex(&stream_lock);

    while ((packets_sent < STREAM_TX_PACKETS_PER_POLL) && Stream_DequeuePacket(&packet))
    {
        Protocol_SendStreamBatch(packet.stream_id, packet.sample_count, packet.payload, packet.payload_len);
        packets_sent++;
    }
}

static bool Stream_QueuePacket(const StreamPacket_t *packet)
{
    if ((packet == NULL) || (packet->sample_count == 0U) || (packet->payload_len > sizeof(packet_queue[0].payload)))
    {
        return false;
    }

    if (packet_count == STREAM_PACKET_BUFFER_SIZE)
    {
        packet_tail = (uint8_t)((packet_tail + 1U) % STREAM_PACKET_BUFFER_SIZE);
        packet_count--;
    }

    packet_queue[packet_head] = *packet;

    packet_head = (uint8_t)((packet_head + 1U) % STREAM_PACKET_BUFFER_SIZE);
    packet_count++;
    return true;
}

static bool Stream_DequeuePacket(StreamPacket_t *packet)
{
    if (packet == NULL)
    {
        return false;
    }

    Stream_Lock();

    if (packet_count == 0U)
    {
        IfxCpu_releaseMutex(&stream_lock);
        return false;
    }

    *packet = packet_queue[packet_tail];
    packet_tail = (uint8_t)((packet_tail + 1U) % STREAM_PACKET_BUFFER_SIZE);
    packet_count--;
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

static void Stream_PurgePackets(uint8_t stream_id)
{
    StreamPacket_t kept[STREAM_PACKET_BUFFER_SIZE];
    uint8_t kept_count = 0U;
    uint8_t read_idx = packet_tail;

    for (uint8_t i = 0U; i < packet_count; i++)
    {
        StreamPacket_t packet = packet_queue[read_idx];
        if (packet.stream_id != stream_id)
        {
            kept[kept_count++] = packet;
        }

        read_idx = (uint8_t)((read_idx + 1U) % STREAM_PACKET_BUFFER_SIZE);
    }

    packet_head = 0U;
    packet_tail = 0U;
    packet_count = 0U;

    for (uint8_t i = 0U; i < kept_count; i++)
    {
        packet_queue[packet_head] = kept[i];
        packet_head = (uint8_t)((packet_head + 1U) % STREAM_PACKET_BUFFER_SIZE);
        packet_count++;
    }
}

static void Stream_AppendFrame(StreamInfo_t *stream, const StreamFrame_t *frame, uint32_t current_loop_count)
{
    uint16_t sample_bytes;

    if ((stream == NULL) || (frame == NULL))
    {
        return;
    }

    sample_bytes = (uint16_t)(STREAM_SAMPLE_HEADER_BYTES + frame->payload_len);

    if (stream->batch_target_samples == 0U)
    {
        return;
    }

    if ((stream->batch_sample_count > 0U) &&
        ((stream->batch_sample_count >= stream->batch_target_samples) ||
         ((uint16_t)(stream->batch_payload_len + sample_bytes) > sizeof(stream->batch_payload))))
    {
        Stream_FlushBatch(stream);
    }

    if (stream->batch_sample_count == 0U)
    {
        stream->batch_first_loop_count = current_loop_count;
    }

    stream->batch_payload[stream->batch_payload_len++] = (uint8_t)(frame->sequence & 0xFFU);
    stream->batch_payload[stream->batch_payload_len++] = (uint8_t)((frame->sequence >> 8) & 0xFFU);

    for (uint8_t i = 0U; i < 8U; i++)
    {
        stream->batch_payload[stream->batch_payload_len++] = (uint8_t)((frame->timestamp_ticks >> (8U * i)) & 0xFFU);
    }

    if (frame->payload_len > 0U)
    {
        memcpy(&stream->batch_payload[stream->batch_payload_len], frame->payload, frame->payload_len);
        stream->batch_payload_len = (uint16_t)(stream->batch_payload_len + frame->payload_len);
    }

    stream->batch_sample_count++;

    if (stream->batch_sample_count >= stream->batch_target_samples)
    {
        Stream_FlushBatch(stream);
    }
}

static void Stream_FlushBatch(StreamInfo_t *stream)
{
    StreamPacket_t packet;

    if ((stream == NULL) || (stream->batch_sample_count == 0U))
    {
        return;
    }

    packet.stream_id = stream->id;
    packet.sample_count = stream->batch_sample_count;
    packet.payload_len = stream->batch_payload_len;
    memcpy(packet.payload, stream->batch_payload, stream->batch_payload_len);

    (void)Stream_QueuePacket(&packet);

    stream->batch_sample_count = 0U;
    stream->batch_payload_len = 0U;
    stream->batch_first_loop_count = 0U;
}

static void Stream_FlushStaleBatches(uint32_t current_loop_count)
{
    for (int i = 0; i < (int)STREAM_MAX_STREAMS; i++)
    {
        if (streams[i].active && (streams[i].batch_sample_count > 0U) &&
            ((uint32_t)(current_loop_count - streams[i].batch_first_loop_count) >= streams[i].batch_max_age_loops))
        {
            Stream_FlushBatch(&streams[i]);
        }
    }
}

static uint8_t Stream_MaxSamplesPerPacket(uint8_t payload_len)
{
    uint16_t sample_bytes = (uint16_t)(STREAM_SAMPLE_HEADER_BYTES + payload_len);
    uint16_t max_samples;

    if (sample_bytes == 0U)
    {
        return 0U;
    }

    max_samples = (uint16_t)(STREAM_PACKET_PAYLOAD_BYTES / sample_bytes);
    if (max_samples > 255U)
    {
        max_samples = 255U;
    }

    return (uint8_t)max_samples;
}

static uint8_t Stream_TargetSamplesPerPacket(uint8_t loop_divider, uint8_t payload_len)
{
    uint8_t max_samples = Stream_MaxSamplesPerPacket(payload_len);
    uint16_t latency_limited_samples;

    if ((max_samples == 0U) || (loop_divider == 0U))
    {
        return 0U;
    }

    latency_limited_samples = (uint16_t)((STREAM_BATCH_TARGET_LATENCY_LOOPS + (uint32_t)loop_divider - 1U) /
                                         (uint32_t)loop_divider);
    if (latency_limited_samples == 0U)
    {
        latency_limited_samples = 1U;
    }

    if (latency_limited_samples > max_samples)
    {
        latency_limited_samples = max_samples;
    }

    return (uint8_t)latency_limited_samples;
}
