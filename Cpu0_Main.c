#include "adc.h"
#include <inverter/pwm/phase_pwm.h>
#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "Bsp.h"
#include "IfxPort_Pinmap.h"
#include "stdio.h"
#include "logger.h"
#include "gate_driver.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define WAIT_TIME   500              /* Number of milliseconds to wait between each duty cycle change                */


IFX_ALIGN(4) IfxCpu_syncEvent g_cpuSyncEvent = 0;

void core0_main(void)
{
    IfxCpu_enableInterrupts();
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());

    /* Wait for CPU sync event */
    IfxCpu_emitEvent(&g_cpuSyncEvent);
    IfxCpu_waitEvent(&g_cpuSyncEvent, 1);

    gatedriverInit();
    adcInit();
    inverterInit();
    controller_init();

    gatedriverReadyMode();
    gatedriverEnable();

    logSysClocks();
    while(1)
    {
       /* Print a keep-alive message every ~1 second (assuming loop runs fast)
          or just rely on the wait */
       //wait(IfxStm_getTicksFromMilliseconds(BSP_DEFAULT_TIMER, 1000));
    }
}


