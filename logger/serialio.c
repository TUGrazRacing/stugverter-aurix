/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "serialio.h"
#include "IfxAsclin_bf.h"
#include <io.h>

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/
void SERIALIO_Init (sint32 baudrate)
{
    /* Create an instance of the ASC handle */
    IfxAsclin_Asc ascHandle;

    /* Initialize an instance of IfxAsclin_Asc_Config with default values */
    IfxAsclin_Asc_Config ascConfig;
    IfxAsclin_Asc_initModuleConfig(&ascConfig, SERIALIO.asclin);

    ascConfig.baudrate.baudrate = (float32)baudrate;                            /* Set the desired baud rate         */

    /* Pin configuration */
    const IfxAsclin_Asc_Pins pins = {
        NULL_PTR, IfxPort_InputMode_pullUp,                                     /* CTS pin not used                  */
        SERIALIO.rx_pin, IfxPort_InputMode_pullUp,                              /* RX pin                            */
        NULL_PTR, IfxPort_OutputMode_pushPull,                                  /* RTS pin not used                  */
        SERIALIO.tx_pin, IfxPort_OutputMode_pushPull,                           /* TX pin                            */
        IfxPort_PadDriver_cmosAutomotiveSpeed1};
    ascConfig.pins = &pins;

    /* Initialize module with above parameters */
    IfxAsclin_Asc_initModule(&ascHandle, &ascConfig);

    /* Set the Transmit FIFO Level Flag (TFL) via the FLAGSSET register to start the transmission */
    SERIALIO.asclin->FLAGSSET.B.TFLS = 1;
}

void _io_putc (int c, struct _io *io)
{
    if (io->fp == NULL)
    {
        /* Called on print on string */
        /* If we still have enough space in the string */
        if (io->ptr < io->end)
        {
            *(io->ptr++) = (char)c;
        }
    }
    else
    {
        /* --- FIX: Insert CR before LF --- */
        if (c == '\n')
        {
             while (IfxAsclin_getTxFifoFillLevelFlagStatus(SERIALIO.asclin) != TRUE);
             IfxAsclin_clearTxFifoFillLevelFlag(SERIALIO.asclin);
             IfxAsclin_writeTxData(SERIALIO.asclin, '\r');
        }
        /* -------------------------------- */

        while (IfxAsclin_getTxFifoFillLevelFlagStatus(SERIALIO.asclin) != TRUE)
        {}

        IfxAsclin_clearTxFifoFillLevelFlag(SERIALIO.asclin);

        /* Send the character */
        IfxAsclin_writeTxData(SERIALIO.asclin, c);
    }
}

int _io_getc(struct _io *io)
{
    if (io->fp != NULL)
    {
        while (IfxAsclin_getRxFifoFillLevelFlagStatus(SERIALIO.asclin) != TRUE)
        {}

        IfxAsclin_clearRxFifoFillLevelFlag(SERIALIO.asclin);

        /* Read the character */
        return IfxAsclin_readRxData(SERIALIO.asclin);
    }
    else
    {
        __debug();
    }
    return EOF;
}
