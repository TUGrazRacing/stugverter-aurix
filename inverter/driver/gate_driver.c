#include "gate_driver.h"
#include <gtm/gtm.h>
#include <Port/Io/IfxPort_Io.h>
#include "IfxPort_Pinmap.h"
#include "IfxStm.h"
#include "UART_Logging.h"
#include <stdio.h>

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

#define GATEDRIVER_DATA_PRINT_PERIOD_MS (100U)


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

static const char *const g_gatedriverDataChannelNames[GTM_DRIVER_DATA_CHANNEL_COUNT] =
{
    "UL",
    "VL",
    "WL",
    "UH",
    "VH",
    "WH"
};

static uint64 g_gatedriverDataLastPrintTick;
static uint64 g_gatedriverDataPrintPeriodTicks;

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

void gatedriverDataCapture(void)
{
    (void)gtmDriverDataTimUpdate();
}

void gatedriverDataServiceInit(void)
{
    initUART();
    gtmDriverDataTimInit();

    g_gatedriverDataLastPrintTick = (uint64)IfxStm_get(&MODULE_STM0);
    g_gatedriverDataPrintPeriodTicks =
        (uint64)IfxStm_getTicksFromMilliseconds(&MODULE_STM0, GATEDRIVER_DATA_PRINT_PERIOD_MS);
}

void gatedriverDataServiceTask(void)
{
    uint64 now;

    (void)gtmDriverDataTimProcess();

    now = (uint64)IfxStm_get(&MODULE_STM0);
    if((now - g_gatedriverDataLastPrintTick) >= g_gatedriverDataPrintPeriodTicks)
    {
        uint32 channel;

        g_gatedriverDataLastPrintTick = now;

        for(channel = 0U; channel < GTM_DRIVER_DATA_CHANNEL_COUNT; channel++)
        {
            GtmDriverDataChannelState state;

            if(gtmDriverDataTimCopyChannel((GtmDriverDataChannelId)channel, &state) == FALSE)
            {
                continue;
            }

            if(state.pwm.initialized == FALSE)
            {
                continue;
            }

            {
                char message[160];
                int len = snprintf(message,
                                   sizeof(message),
                                   "GD %s adc=%u diag0=0x%04X diag1=0x%04X valid=%u%u%u raw=0x%04X frames=%lu lost=%lu inv=%lu\r\n",
                                   g_gatedriverDataChannelNames[channel],
                                   (unsigned int)state.readout.adc,
                                   (unsigned int)state.readout.diagnosticFrame0,
                                   (unsigned int)state.readout.diagnosticFrame1,
                                   (unsigned int)state.readout.adcValid,
                                   (unsigned int)state.readout.diagnosticFrame0Valid,
                                   (unsigned int)state.readout.diagnosticFrame1Valid,
                                   (unsigned int)state.readout.lastRawFrame,
                                   (unsigned long)state.frameCount,
                                   (unsigned long)state.dataLostCount,
                                   (unsigned long)state.invalidFrameCount);

                if(len > 0)
                {
                    if((unsigned int)len >= sizeof(message))
                    {
                        len = (int)(sizeof(message) - 1U);
                        message[len] = '\0';
                    }

                    sendUARTMessage(message, (Ifx_SizeT)len);
                }
            }
        }
    }
}

void gatedriverDataServiceRun(void)
{
    while(1)
    {
        gatedriverDataServiceTask();
    }
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
