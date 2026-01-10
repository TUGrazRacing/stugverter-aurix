#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "serialio.h"
#include "Bsp.h"
#include <Port/Io/IfxPort_Io.h>
#include "pwm.h"
#include "IfxPort_Pinmap.h"
#include "stdio.h"
#include "adc.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define WAIT_TIME   500              /* Number of milliseconds to wait between each duty cycle change                */
#define UART_BAUDRATE           115200                                  /* UART baud rate in bit/s                   */


IFX_ALIGN(4) IfxCpu_syncEvent g_cpuSyncEvent = 0;

SERIALIO_t SERIALIO =
{
  .asclin = &MODULE_ASCLIN0,
  .tx_pin = &IfxAsclin0_TX_P14_0_OUT,
  .rx_pin = &IfxAsclin0_RXA_P14_1_IN
};


void core0_main(void)
{
    IfxCpu_enableInterrupts();
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());

    /* Wait for CPU sync event */
    IfxCpu_emitEvent(&g_cpuSyncEvent);
    IfxCpu_waitEvent(&g_cpuSyncEvent, 1);

    SERIALIO_Init(UART_BAUDRATE);

    /* Initialize a time variable */
    //Ifx_TickTime ticksFor500ms = IfxStm_getTicksFromMilliseconds(BSP_DEFAULT_TIMER, WAIT_TIME);
    /* Initialize GTM ATOM module */
    // FOC_InitializeSinCosLut();
    // initADCTrigger();
    //initEvadc();
    // init gate driver
    const IfxPort_Io_ConfigPin configPin[] = {
        {&IfxPort_P21_2, IfxPort_Mode_outputPushPullGeneral, IfxPort_PadDriver_cmosAutomotiveSpeed1}, //NRST W
        {&IfxPort_P14_8, IfxPort_Mode_outputPushPullGeneral, IfxPort_PadDriver_cmosAutomotiveSpeed1}, //NRST V
        {&IfxPort_P14_6, IfxPort_Mode_outputPushPullGeneral, IfxPort_PadDriver_cmosAutomotiveSpeed1}, //NRST U
        {&IfxPort_P21_3, IfxPort_Mode_outputPushPullGeneral, IfxPort_PadDriver_cmosAutomotiveSpeed1}, //Enable W
        {&IfxPort_P14_7, IfxPort_Mode_outputPushPullGeneral, IfxPort_PadDriver_cmosAutomotiveSpeed1}, //Enable V
        {&IfxPort_P20_0, IfxPort_Mode_outputPushPullGeneral, IfxPort_PadDriver_cmosAutomotiveSpeed1}, //Enable U
    };

    const IfxPort_Io_Config conf = {
        sizeof(configPin)/sizeof(IfxPort_Io_ConfigPin),
        (IfxPort_Io_ConfigPin *)configPin
    };

    IfxPort_Io_initModule(&conf);

    initGtmAtom3phInv();
    initEvadc();




    /*
    //ENABLES LOW
    IfxPort_setPinLow(&MODULE_P21, 3);
    IfxPort_setPinLow(&MODULE_P14, 7);
    IfxPort_setPinLow(&MODULE_P20, 0);

    //RESET W
    IfxPort_setPinHigh(&MODULE_P21, 2);
    IfxPort_setPinLow(&MODULE_P21, 2);
    IfxPort_setPinHigh(&MODULE_P21, 2);

    //RESET V
    IfxPort_setPinHigh(&MODULE_P14, 8);
    IfxPort_setPinLow(&MODULE_P14, 8);
    IfxPort_setPinHigh(&MODULE_P14, 8);

    //RESET U
    IfxPort_setPinHigh(&MODULE_P14, 6);
    IfxPort_setPinLow(&MODULE_P14, 6);
    IfxPort_setPinHigh(&MODULE_P14, 6);

    //ENABLES HIGH
   IfxPort_setPinHigh(&MODULE_P21, 3);
   IfxPort_setPinHigh(&MODULE_P14, 7);
   IfxPort_setPinHigh(&MODULE_P20, 0);
*/
   /* Delay for 3 seconds (10000 ms) */
   /*
   wait(IfxStm_getTicksFromMilliseconds(BSP_DEFAULT_TIMER, 3000));

   //ENABLES LOW
   IfxPort_setPinLow(&MODULE_P21, 3);
   IfxPort_setPinLow(&MODULE_P14, 7);
   IfxPort_setPinLow(&MODULE_P20, 0);
    */
   while(1)
   {
       /* Print a keep-alive message every ~1 second (assuming loop runs fast)
          or just rely on the wait */

       //wait(IfxStm_getTicksFromMilliseconds(BSP_DEFAULT_TIMER, 1000));
   }
}


