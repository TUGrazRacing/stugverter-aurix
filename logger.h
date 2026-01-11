/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#ifndef LOGGER_H
#define LOGGER_H

#include "Ifx_Types.h"
#include "IfxStm.h"

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/
/* Extern declarations */
extern uint64 start_ticks;
extern uint64 stop_ticks;

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief Logs important system clocks (CPU, STM, ADC) to the console.
 */
void logSysClocks(void);

/**
 * @brief Calculates the delta between start/stop ticks, converts to us, and prints.
 */
void logTimeDiff(void);

/**
 * @brief Blocking delay in microseconds using the STM.
 * Added based on IfxStm documentation capability.
 * @param delayUs Microseconds to wait
 */
void logDelayUs(uint32 delayUs);

/*********************************************************************************************************************/
/*---------------------------------------------Inline Implementations------------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief Captures the current STM tick count.
 */
static inline void logStart(void)
{
    /* Use the default timer macro from IfxStm.h */
    start_ticks = IfxStm_get(IFXSTM_DEFAULT_TIMER);
}

/**
 * @brief Captures the current STM tick count.
 */
static inline void logEnd(void)
{
    /* Use the default timer macro from IfxStm.h */
    stop_ticks = IfxStm_get(IFXSTM_DEFAULT_TIMER);
}

#endif /* LOGGER_H */
