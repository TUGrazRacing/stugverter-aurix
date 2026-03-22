#include "gate_driver.h"
#include <Port/Io/IfxPort_Io.h>
#include "IfxPort_Pinmap.h"
#include "IfxStm.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
/* Pin Definitions */
#define GATEDRIVER_NRST_U   (&IfxPort_P14_6)
#define GATEDRIVER_NRST_V   (&IfxPort_P14_8)
#define GATEDRIVER_NRST_W   (&IfxPort_P21_2)
#define GATEDRIVER_EN_U     (&IfxPort_P20_0)
#define GATEDRIVER_EN_V     (&IfxPort_P14_7)
#define GATEDRIVER_EN_W     (&IfxPort_P21_3)

/*********************************************************************************************************************/
/*------------------------------------------------Global Variables---------------------------------------------------*/
/*********************************************************************************************************************/
/* Configuration structure for the Port IO module */
const IfxPort_Io_ConfigPin gatedriver_config[] = {
    {GATEDRIVER_NRST_U, IfxPort_Mode_outputPushPullGeneral, IfxPort_PadDriver_cmosAutomotiveSpeed1},
    {GATEDRIVER_NRST_V, IfxPort_Mode_outputPushPullGeneral, IfxPort_PadDriver_cmosAutomotiveSpeed1},
    {GATEDRIVER_NRST_W, IfxPort_Mode_outputPushPullGeneral, IfxPort_PadDriver_cmosAutomotiveSpeed1},
    {GATEDRIVER_EN_U,   IfxPort_Mode_outputPushPullGeneral, IfxPort_PadDriver_cmosAutomotiveSpeed1},
    {GATEDRIVER_EN_V,   IfxPort_Mode_outputPushPullGeneral, IfxPort_PadDriver_cmosAutomotiveSpeed1},
    {GATEDRIVER_EN_W,   IfxPort_Mode_outputPushPullGeneral, IfxPort_PadDriver_cmosAutomotiveSpeed1},
};

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

void gatedriverInit(void)
{
    /* 1. SAFETY: Explicitly set the Output Register to LOW before enabling the driver.
     * This prevents the pins from driving High if the default register state was undefined.
     * We access the 'port' and 'pinIndex' fields from the PinMap pointer.
     */
    IfxPort_setPinLow(GATEDRIVER_NRST_U->port, GATEDRIVER_NRST_U->pinIndex);
    IfxPort_setPinLow(GATEDRIVER_NRST_V->port, GATEDRIVER_NRST_V->pinIndex);
    IfxPort_setPinLow(GATEDRIVER_NRST_W->port, GATEDRIVER_NRST_W->pinIndex);

    IfxPort_setPinLow(GATEDRIVER_EN_U->port,   GATEDRIVER_EN_U->pinIndex);
    IfxPort_setPinLow(GATEDRIVER_EN_V->port,   GATEDRIVER_EN_V->pinIndex);
    IfxPort_setPinLow(GATEDRIVER_EN_W->port,   GATEDRIVER_EN_W->pinIndex);

    /* 2. Setup IO Mode (switch pins to Output Push-Pull) */
    const IfxPort_Io_Config conf = {
        sizeof(gatedriver_config)/sizeof(IfxPort_Io_ConfigPin),
        (IfxPort_Io_ConfigPin *)gatedriver_config
    };

    IfxPort_Io_initModule(&conf);
}

void gatedriverReadyMode(void)
{
    /* Pulse Reset: High -> Low -> High to clear faults and latch config */

    /* Set High */
    IfxPort_setPinHigh(GATEDRIVER_NRST_U->port, GATEDRIVER_NRST_U->pinIndex);
    IfxPort_setPinHigh(GATEDRIVER_NRST_V->port, GATEDRIVER_NRST_V->pinIndex);
    IfxPort_setPinHigh(GATEDRIVER_NRST_W->port, GATEDRIVER_NRST_W->pinIndex);

    /* Add a small delay if required by your specific Gate Driver datasheet here */
    IfxStm_wait(IfxStm_getTicksFromMicroseconds(&MODULE_STM0, 5));
    /* Set Low (Reset active) */
    IfxPort_setPinLow(GATEDRIVER_NRST_U->port, GATEDRIVER_NRST_U->pinIndex);
    IfxPort_setPinLow(GATEDRIVER_NRST_V->port, GATEDRIVER_NRST_V->pinIndex);
    IfxPort_setPinLow(GATEDRIVER_NRST_W->port, GATEDRIVER_NRST_W->pinIndex);

    /* Add delay if required */
    IfxStm_wait(IfxStm_getTicksFromMicroseconds(&MODULE_STM0, 5));

    /* Set High (Release Reset) */
    IfxPort_setPinHigh(GATEDRIVER_NRST_U->port, GATEDRIVER_NRST_U->pinIndex);
    IfxPort_setPinHigh(GATEDRIVER_NRST_V->port, GATEDRIVER_NRST_V->pinIndex);
    IfxPort_setPinHigh(GATEDRIVER_NRST_W->port, GATEDRIVER_NRST_W->pinIndex);
}

void gatedriverEnable(void)
{
    /* Set Enables High */
    IfxPort_setPinHigh(GATEDRIVER_EN_U->port, GATEDRIVER_EN_U->pinIndex);
    IfxPort_setPinHigh(GATEDRIVER_EN_V->port, GATEDRIVER_EN_V->pinIndex);
    IfxPort_setPinHigh(GATEDRIVER_EN_W->port, GATEDRIVER_EN_W->pinIndex);
}

void gatedriverDisable(void)
{
    /* Set Enables Low */
    IfxPort_setPinLow(GATEDRIVER_EN_U->port, GATEDRIVER_EN_U->pinIndex);
    IfxPort_setPinLow(GATEDRIVER_EN_V->port, GATEDRIVER_EN_V->pinIndex);
    IfxPort_setPinLow(GATEDRIVER_EN_W->port, GATEDRIVER_EN_W->pinIndex);
}
