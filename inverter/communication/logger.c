/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "logger.h"
#include "IfxScuCcu.h"
#include <stdio.h>
#include "can.h"
#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "IfxStm.h"

volatile uint64 start_ticks = 0;
volatile uint64 stop_ticks = 0;

/* Circular Buffer */
static LogData_t g_logBuffer[LOG_BUFFER_SIZE];
static volatile uint32 g_writeIdx = 0;
static volatile uint32 g_readIdx = 0;

/* CAN send timing */
static uint64 g_periodTicks = 0;
static uint64 g_nextTxTime = 0;
/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

static uint64 periodTicks = 0;
static uint64 nextTxTime  = 0;

void initLogger(void)
{
    g_writeIdx = 0;
    g_readIdx  = 0;

    uint64 stmFreqHz = (uint64)IfxStm_getFrequency(IFXSTM_DEFAULT_TIMER);
    periodTicks = stmFreqHz / 1000;
    nextTxTime  = IfxStm_get(IFXSTM_DEFAULT_TIMER) + periodTicks;
}

boolean logPush(const LogData_t *log)
{
    uint32 nextWriteIdx = (g_writeIdx + 1) % LOG_BUFFER_SIZE;

    if (nextWriteIdx == g_readIdx)
    {
        return FALSE;
    }

    g_logBuffer[g_writeIdx] = *log;
    g_writeIdx = nextWriteIdx;
    return TRUE;
}

void logProcess(void)
{
    uint64 now = IfxStm_get(IFXSTM_DEFAULT_TIMER);

    if ((sint64)(now - nextTxTime) < 0)
    {
        return;
    }

    if (g_readIdx != g_writeIdx)
    {
        LogData_t data = g_logBuffer[g_readIdx];
        g_readIdx = (g_readIdx + 1) % LOG_BUFFER_SIZE;

        transmitCanMessage(data.i_u, data.i_v, data.theta);
    }

    nextTxTime += periodTicks;
}

void logSysClocks (void)
{
  /* IfxStm_getFrequency returns float32 according to docs */
  float cpuFreqHz = (float) IfxScuCcu_getCpuFrequency(IfxCpu_ResourceCpu_0);
  float stmFreqHz = IfxStm_getFrequency(IFXSTM_DEFAULT_TIMER);
  float adcFreqHz = (float) IfxScuCcu_getAdcFrequency();

  printf("=== System Clock Summary ===\n");
  printf("CPU0 Frequency: %8.2f MHz\n", cpuFreqHz / 1000000.0f);
  printf("STM  Frequency: %8.2f MHz\n", stmFreqHz / 1000000.0f);
  printf("ADC  Frequency: %8.2f MHz\n", adcFreqHz / 1000000.0f);
  printf("============================\n\n");
}

void logTimeDiff (void)
{
  /* * Standard unsigned arithmetic handles wrap-around automatically.
   * 0x...01 (stop) - 0x...FF (start) == 2 ticks.
   */
  uint64 ticksTaken = stop_ticks - start_ticks;

  /* Get current STM frequency */
  float stmFreqHz = IfxStm_getFrequency(IFXSTM_DEFAULT_TIMER);

  /* Calculate time in microseconds */
  float timeInUs = ((float) ticksTaken / stmFreqHz) * 1000000.0f;

  /* Printf implementation */
  printf("[Prof] Exec Time: %8.3f us (Ticks: %llu)\n", timeInUs, ticksTaken);
}

void logDelayUs (uint32 delayUs)
{
  /* * Uses the helper functions found in IfxStm.h:
   * IfxStm_getTicksFromMicroseconds
   * IfxStm_waitTicks
   */
  sint32 ticksToWait = IfxStm_getTicksFromMicroseconds(IFXSTM_DEFAULT_TIMER, delayUs);

  if (ticksToWait > 0)
  {
    IfxStm_waitTicks(IFXSTM_DEFAULT_TIMER, (uint32) ticksToWait);
  }
}
