#include "IfxAsclin_Asc.h"
#include <string.h>

/* Communication parameters */
#define ASC_TX_BUFFER_SIZE  64
#define ASC_RX_BUFFER_SIZE  64
#define ASC_BAUDRATE        115200

#define ISR_PRIORITY_ASCLIN_TX  8
#define ISR_PRIORITY_ASCLIN_RX  4
#define ISR_PRIORITY_ASCLIN_ER  12

/* Global ASCLIN module object */
IfxAsclin_Asc g_asclin;

/* Transfer buffers and FIFO runtime variables */
uint8 g_uartTxBuffer[ASC_TX_BUFFER_SIZE + sizeof(Ifx_Fifo) + 8];
uint8 g_uartRxBuffer[ASC_RX_BUFFER_SIZE + sizeof(Ifx_Fifo) + 8];

/* ------------------------------------------------------------------------- *
 * 1. INTERRUPT SERVICE ROUTINES
 * Even in a simple example, the iLLD ASC driver uses interrupts
 * in the background to move data from your buffers to the hardware.
 * ------------------------------------------------------------------------- */
IFX_INTERRUPT(asc0TxISR, 0, ISR_PRIORITY_ASCLIN_TX);
void asc0TxISR(void) {
    IfxAsclin_Asc_isrTransmit(&g_asclin);
}

IFX_INTERRUPT(asc0RxISR, 0, ISR_PRIORITY_ASCLIN_RX);
void asc0RxISR(void) {
    IfxAsclin_Asc_isrReceive(&g_asclin);
}

IFX_INTERRUPT(asc0ErrISR, 0, ISR_PRIORITY_ASCLIN_ER);
void asc0ErrISR(void) {
    IfxAsclin_Asc_isrError(&g_asclin);
}

/* ------------------------------------------------------------------------- *
 * 2. INITIALIZATION
 * ------------------------------------------------------------------------- */
void initSimpleUART(void)
{
    IfxAsclin_Asc_Config ascConf;

    /* Initialize the configuration structure with default values */
    IfxAsclin_Asc_initModuleConfig(&ascConf, &MODULE_ASCLIN0);

    /* Set baud rate */
    ascConf.baudrate.baudrate = ASC_BAUDRATE;

    /* Link the interrupts */
    ascConf.interrupt.txPriority = ISR_PRIORITY_ASCLIN_TX;
    ascConf.interrupt.rxPriority = ISR_PRIORITY_ASCLIN_RX;
    ascConf.interrupt.erPriority = ISR_PRIORITY_ASCLIN_ER;
    ascConf.interrupt.typeOfService = IfxSrc_Tos_cpu0;

    ascConf.baudrate.oversampling = IfxAsclin_OversamplingFactor_16;
    ascConf.bitTiming.medianFilter = IfxAsclin_SamplesPerBit_three;
    ascConf.bitTiming.samplePointPosition = IfxAsclin_SamplePointPosition_8;

    /* Configure Pins (Make sure these match your specific board) */
    const IfxAsclin_Asc_Pins pins = {
        .cts        = NULL_PTR,
        .ctsMode    = IfxPort_InputMode_pullUp,
        .rx         = &IfxAsclin0_RXA_P14_1_IN,
        .rxMode     = IfxPort_InputMode_pullUp,
        .rts        = NULL_PTR,
        .rtsMode    = IfxPort_OutputMode_pushPull,
        .tx         = &IfxAsclin0_TX_P14_0_OUT,
        .txMode     = IfxPort_OutputMode_pushPull,
        .pinDriver  = IfxPort_PadDriver_cmosAutomotiveSpeed1
    };
    ascConf.pins = &pins;

    /* Link FIFO buffers */
    ascConf.txBuffer = g_uartTxBuffer;
    ascConf.txBufferSize = ASC_TX_BUFFER_SIZE;
    ascConf.rxBuffer = g_uartRxBuffer;
    ascConf.rxBufferSize = ASC_RX_BUFFER_SIZE;

    /* Apply configuration to hardware */
    IfxAsclin_Asc_initModule(&g_asclin, &ascConf);
}

/* ------------------------------------------------------------------------- *
 * 3. ECHO FUNCTION
 * ------------------------------------------------------------------------- */
void echoUART(void)
{
    uint8 rxData = 0;
    Ifx_SizeT count = 1;

    // TIME_INFINITE blocks the execution until exactly 1 byte is received
    if (IfxAsclin_Asc_read(&g_asclin, &rxData, &count, TIME_INFINITE))
    {
        // Once a byte is received, immediately write it back to the terminal */
        IfxAsclin_Asc_write(&g_asclin, &rxData, &count, TIME_INFINITE);

        // Optional: If you press 'Enter' in a terminal, it usually sends a Carriage Return '\r'.
        // To make it display correctly on the next line, you might want to echo a Line Feed '\n' too. */
        if (rxData == '\r')
        {
            uint8 newLine = '\n';
            IfxAsclin_Asc_write(&g_asclin, &newLine, &count, TIME_INFINITE);
        }
    }
}
