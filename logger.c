/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "logger.h"
#include "IfxScuCcu.h"
#include <stdio.h>


/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/
uint64 start_ticks = 0;
uint64 stop_ticks = 0;

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

void logSysClocks(void)
{
    /* IfxStm_getFrequency returns float32 according to docs */
    float cpuFreqHz = (float)IfxScuCcu_getCpuFrequency(IfxCpu_ResourceCpu_0);
    float stmFreqHz = IfxStm_getFrequency(IFXSTM_DEFAULT_TIMER);
    float adcFreqHz = (float)IfxScuCcu_getAdcFrequency();

    printf("=== System Clock Summary ===\n");
    printf("CPU0 Frequency: %8.2f MHz\n", cpuFreqHz / 1000000.0f);
    printf("STM  Frequency: %8.2f MHz\n", stmFreqHz / 1000000.0f);
    printf("ADC  Frequency: %8.2f MHz\n", adcFreqHz / 1000000.0f);
    printf("============================\n\n");
}

void logTimeDiff(void)
{
    /* * Standard unsigned arithmetic handles wrap-around automatically.
     * 0x...01 (stop) - 0x...FF (start) == 2 ticks.
     */
    uint64 ticksTaken = stop_ticks - start_ticks;

    /* Get current STM frequency */
    float stmFreqHz = IfxStm_getFrequency(IFXSTM_DEFAULT_TIMER);

    /* Calculate time in microseconds */
    float timeInUs = ((float)ticksTaken / stmFreqHz) * 1000000.0f;

    /* Printf implementation */
    printf("[Prof] Exec Time: %8.3f us (Ticks: %llu)\n", timeInUs, ticksTaken);
}

void logDelayUs(uint32 delayUs)
{
    /* * Uses the helper functions found in IfxStm.h:
     * IfxStm_getTicksFromMicroseconds
     * IfxStm_waitTicks
     */
    sint32 ticksToWait = IfxStm_getTicksFromMicroseconds(IFXSTM_DEFAULT_TIMER, delayUs);

    if (ticksToWait > 0)
    {
        IfxStm_waitTicks(IFXSTM_DEFAULT_TIMER, (uint32)ticksToWait);
    }
}
