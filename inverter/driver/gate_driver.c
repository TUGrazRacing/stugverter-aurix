#include "gate_driver.h"
#include <Port/Io/IfxPort_Io.h>
#include "IfxPort_Pinmap.h"
#include "IfxStm.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
/* Pin Definitions */
#define GATEDRIVER_EN_U_LOW   (&IfxPort_P14_6)
#define GATEDRIVER_EN_V_LOW   (&IfxPort_P14_8)
#define GATEDRIVER_EN_W_LOW   (&IfxPort_P21_2)
#define GATEDRIVER_EN_U_HIGH     (&IfxPort_P20_0)
#define GATEDRIVER_EN_V_HIGH     (&IfxPort_P14_7)
#define GATEDRIVER_EN_W_HIGH     (&IfxPort_P21_3)

#define GATEDRIVER_DATA_U_LOW
#define GATEDRIVER_DATA_V_LOW
#define GATEDRIVER_DATA_W_LOW
#define GATEDRIVER_DATA_U_HIGH
#define GATEDRIVER_DATA_V_HIGH
#define GATEDRIVER_DATA_W_HIGH


/*********************************************************************************************************************/
/*------------------------------------------------Global Variables---------------------------------------------------*/
/*********************************************************************************************************************/
/* Configuration structure for the Port IO module */
const IfxPort_Io_ConfigPin gatedriver_config[] = {
    {GATEDRIVER_EN_U_LOW, IfxPort_Mode_outputPushPullGeneral, IfxPort_PadDriver_cmosAutomotiveSpeed1},
    {GATEDRIVER_EN_V_LOW, IfxPort_Mode_outputPushPullGeneral, IfxPort_PadDriver_cmosAutomotiveSpeed1},
    {GATEDRIVER_EN_W_LOW, IfxPort_Mode_outputPushPullGeneral, IfxPort_PadDriver_cmosAutomotiveSpeed1},
    {GATEDRIVER_EN_U_HIGH,   IfxPort_Mode_outputPushPullGeneral, IfxPort_PadDriver_cmosAutomotiveSpeed1},
    {GATEDRIVER_EN_V_HIGH,   IfxPort_Mode_outputPushPullGeneral, IfxPort_PadDriver_cmosAutomotiveSpeed1},
    {GATEDRIVER_EN_W_HIGH,   IfxPort_Mode_outputPushPullGeneral, IfxPort_PadDriver_cmosAutomotiveSpeed1},
};

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

void gatedriverDataCapture(void)
{

}

void gatedriverInit(void)
{
    /* 1. SAFETY: Explicitly set the Output Register to LOW before enabling the driver.
     * This prevents the pins from driving High if the default register state was undefined.
     * We access the 'port' and 'pinIndex' fields from the PinMap pointer.
     */
    IfxPort_setPinLow(GATEDRIVER_EN_U_HIGH->port,   GATEDRIVER_EN_U_HIGH->pinIndex);
    IfxPort_setPinLow(GATEDRIVER_EN_V_HIGH->port,   GATEDRIVER_EN_V_HIGH->pinIndex);
    IfxPort_setPinLow(GATEDRIVER_EN_W_HIGH->port,   GATEDRIVER_EN_W_HIGH->pinIndex);

    IfxPort_setPinLow(GATEDRIVER_EN_U_LOW->port, GATEDRIVER_EN_U_LOW->pinIndex);
    IfxPort_setPinLow(GATEDRIVER_EN_V_LOW->port, GATEDRIVER_EN_V_LOW->pinIndex);
    IfxPort_setPinLow(GATEDRIVER_EN_W_LOW->port, GATEDRIVER_EN_W_LOW->pinIndex);


    /* 2. Setup IO Mode (switch pins to Output Push-Pull) */
    const IfxPort_Io_Config conf = {
        sizeof(gatedriver_config)/sizeof(IfxPort_Io_ConfigPin),
        (IfxPort_Io_ConfigPin *)gatedriver_config
    };

    IfxPort_Io_initModule(&conf);
}

void gatedriverEnable(void)
{
    IfxPort_setPinHigh(GATEDRIVER_EN_U_HIGH->port,   GATEDRIVER_EN_U_HIGH->pinIndex);
    IfxPort_setPinHigh(GATEDRIVER_EN_V_HIGH->port,   GATEDRIVER_EN_V_HIGH->pinIndex);
    IfxPort_setPinHigh(GATEDRIVER_EN_W_HIGH->port,   GATEDRIVER_EN_W_HIGH->pinIndex);

    IfxPort_setPinHigh(GATEDRIVER_EN_U_LOW->port, GATEDRIVER_EN_U_LOW->pinIndex);
    IfxPort_setPinHigh(GATEDRIVER_EN_V_LOW->port, GATEDRIVER_EN_V_LOW->pinIndex);
    IfxPort_setPinHigh(GATEDRIVER_EN_W_LOW->port, GATEDRIVER_EN_W_LOW->pinIndex);
}

void gatedriverDisable(void)
{
    IfxPort_setPinLow(GATEDRIVER_EN_U_HIGH->port,   GATEDRIVER_EN_U_HIGH->pinIndex);
    IfxPort_setPinLow(GATEDRIVER_EN_V_HIGH->port,   GATEDRIVER_EN_V_HIGH->pinIndex);
    IfxPort_setPinLow(GATEDRIVER_EN_W_HIGH->port,   GATEDRIVER_EN_W_HIGH->pinIndex);

    IfxPort_setPinLow(GATEDRIVER_EN_U_LOW->port, GATEDRIVER_EN_U_LOW->pinIndex);
    IfxPort_setPinLow(GATEDRIVER_EN_V_LOW->port, GATEDRIVER_EN_V_LOW->pinIndex);
    IfxPort_setPinLow(GATEDRIVER_EN_W_LOW->port, GATEDRIVER_EN_W_LOW->pinIndex);
}
