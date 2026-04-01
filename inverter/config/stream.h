#ifndef STREAM_H
#define STREAM_H

#include <stdint.h>
#include <stdbool.h>

/* Configuration */
#define STREAM_MAX_STREAMS 4   // Maximum concurrent active streams
#define STREAM_MAX_REGS    16  // Maximum registers allowed in a single stream

/* Function Prototypes */
void Stream_Init(void);
void Stream_Process(void);
bool Stream_Start(uint8_t id, uint16_t freq_x100, const uint16_t *regs, uint8_t num_regs);
bool Stream_Stop(uint8_t id);

#endif /* STREAM_H */
