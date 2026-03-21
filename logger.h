#ifndef LOGGER_H
#define LOGGER_H

#include "Ifx_Types.h"
#include "IfxStm.h"

/*********************************************************************************************************************/
/*--------------------------------------------------Macros & Types---------------------------------------------------*/
/*********************************************************************************************************************/
#define LOG_BUFFER_SIZE  1024  /* Must be a power of 2 for efficient masking if needed, but simple size is fine */

typedef struct
{
    float32 i_u;
    float32 i_v;
    float32 theta;
} LogData_t;

/*********************************************************************************************************************/
/*------------------------------------------------Global Variables---------------------------------------------------*/
/*********************************************************************************************************************/
extern volatile uint64 start_ticks;
extern volatile uint64 stop_ticks;

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/

/* Initialization */
void logSysClocks(void);
void initLogger(void);

/* Real-Time Profiling (Existing) */
void logTimeDiff(void);
void logDelayUs(uint32 delayUs);
static inline void logStart(void) { start_ticks = IfxStm_get(IFXSTM_DEFAULT_TIMER); }
static inline void logEnd(void)   { stop_ticks  = IfxStm_get(IFXSTM_DEFAULT_TIMER); }

/* Data Logging (New) */

/**
 * @brief Push a new sample into the ring buffer. Safe to call from ISR.
 * @return TRUE if success, FALSE if buffer full.
 */
boolean logPush(const LogData_t *log);

/**
 * @brief Process the buffer and print data. Call this in the Main Loop.
 * Prints up to 'maxItems' at a time to avoid starving other background tasks.
 */
void logProcess(void);

#endif /* LOGGER_H */
