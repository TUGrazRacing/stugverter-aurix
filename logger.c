/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "logger.h"
#include "IfxScuCcu.h"
#include <stdio.h>


/*********************************************************************************************************************/
/*------------------------------------------------Global Variables---------------------------------------------------*/
/*********************************************************************************************************************/
volatile uint64 start_ticks = 0;
volatile uint64 stop_ticks = 0;

/* Circular Buffer */
static LogData_t g_logBuffer[LOG_BUFFER_SIZE];
static volatile uint32 g_writeIdx = 0;
static volatile uint32 g_readIdx = 0;

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

void initLogger(void)
{
    g_writeIdx = 0;
    g_readIdx = 0;
    printf("[Logger] Initialized ring buffer size: %d\n", LOG_BUFFER_SIZE);
}

//int sample_nr = 0;

boolean logPush(const LogData_t *log)
{
//    if(sample_nr++ < 100) return FALSE;
    uint32 nextWriteIdx = (g_writeIdx + 1) % LOG_BUFFER_SIZE;

    /* Check if buffer is full (Next write would catch up to Read) */
    if (nextWriteIdx == g_readIdx)
    {
        /* Buffer Overflow! Drop sample or set an error flag */
        return FALSE;
    }
    g_logBuffer[g_writeIdx] = *log;

    g_writeIdx = nextWriteIdx;
//    sample_nr = 0;

    return TRUE;
}

void logProcess(void)
{
    /* Process chunks of data to allow other main loop tasks to run */
    uint32 count = 0;
    const uint32 MAX_PRINT_PER_LOOP = 10;

    while ((g_readIdx != g_writeIdx) && (count < MAX_PRINT_PER_LOOP))
    {
        LogData_t *data = &g_logBuffer[g_readIdx];

        /* Print the data (Slow operation) */
        /* Format: [ADC] U: 1234, V: 2345 */
        printf("%.2f %.2f %.2f\n", data->i_u, data->i_v, data->theta);

        /* Advance Read Index */
        g_readIdx = (g_readIdx + 1) % LOG_BUFFER_SIZE;
        count++;
    }
}

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
