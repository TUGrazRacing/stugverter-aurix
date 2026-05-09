#include "gate_driver.h"
#include <gtm/gtm.h>
#include "app_config.h"
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

#if APP_GATE_DRIVER_DATA_CHANNEL_COUNT != GTM_DRIVER_DATA_CHANNEL_COUNT
#error "App gate-driver DATA channel count must match the GTM TIM channel count."
#endif

static void gatedriverDataPublishChannel(uint32 channel, const GtmDriverDataChannelState *state);
static boolean gatedriverDataPrintDue(void);
static void gatedriverDataPrintChannel(uint32 channel, const GtmDriverDataChannelState *state);
static void gatedriverDataPoll(void);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief Compatibility wrapper for older call sites that polled the DATA pin directly.
 */
void gatedriverDataCapture(void)
{
    (void)gtmDriverDataTimUpdate();
}

/**
 * @brief Initializes UART, GTM TIM capture, and the core3 print cadence.
 */
void gatedriverDataServiceInit(void)
{
    initUART();
    gtmDriverDataTimInit();

    g_gatedriverDataLastPrintTick = (uint64)IfxStm_get(&MODULE_STM0);
    g_gatedriverDataPrintPeriodTicks =
        (uint64)IfxStm_getTicksFromMilliseconds(&MODULE_STM0, GATEDRIVER_DATA_PRINT_PERIOD_MS);
}

/**
 * @brief Polls all configured DATA channels, publishes their values, and prints periodically.
 */
void gatedriverDataServiceTask(void)
{
    gatedriverDataPoll();
}

/**
 * @brief Runs the gate-driver DATA service forever on core3.
 */
void gatedriverDataServiceRun(void)
{
    while(1)
    {
        gatedriverDataServiceTask();
    }
}

/**
 * @brief Copies the last valid DATA readout into app_state for Ethernet parameters.
 */
static void gatedriverDataPublishChannel(uint32 channel, const GtmDriverDataChannelState *state)
{
    GateDriverDataReadoutState *published = &app_state.gate_driver_data.channel[channel];

    published->adc = state->readout.adc;
    published->diagnosticFrame0 = state->readout.diagnosticFrame0;
    published->diagnosticFrame1 = state->readout.diagnosticFrame1;
}

/**
 * @brief Returns TRUE once the configured UART print period has elapsed.
 */
static boolean gatedriverDataPrintDue(void)
{
    uint64 now = (uint64)IfxStm_get(&MODULE_STM0);

    if((now - g_gatedriverDataLastPrintTick) < g_gatedriverDataPrintPeriodTicks)
    {
        return FALSE;
    }

    g_gatedriverDataLastPrintTick = now;
    return TRUE;
}

/**
 * @brief Prints one initialized DATA channel for bench bring-up.
 */
static void gatedriverDataPrintChannel(uint32 channel, const GtmDriverDataChannelState *state)
{
    char message[160];
    int len;

    len = snprintf(message,
                   sizeof(message),
                   "GD %s adc=%u diag0=0x%04X diag1=0x%04X valid=%u%u%u raw=0x%04X frames=%lu lost=%lu inv=%lu\r\n",
                   g_gatedriverDataChannelNames[channel],
                   (unsigned int)state->readout.adc,
                   (unsigned int)state->readout.diagnosticFrame0,
                   (unsigned int)state->readout.diagnosticFrame1,
                   (unsigned int)state->readout.adcValid,
                   (unsigned int)state->readout.diagnosticFrame0Valid,
                   (unsigned int)state->readout.diagnosticFrame1Valid,
                   (unsigned int)state->readout.lastRawFrame,
                   (unsigned long)state->frameCount,
                   (unsigned long)state->dataLostCount,
                   (unsigned long)state->invalidFrameCount);

    if(len <= 0)
    {
        return;
    }

    if((unsigned int)len >= sizeof(message))
    {
        len = (int)(sizeof(message) - 1U);
        message[len] = '\0';
    }

    sendUARTMessage(message, (Ifx_SizeT)len);
}

/**
 * @brief Polls TIM, publishes all active channels, and emits optional UART diagnostics.
 */
static void gatedriverDataPoll(void)
{
    boolean printDue;
    uint32 channel;

    (void)gtmDriverDataTimProcess();
    printDue = gatedriverDataPrintDue();

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

        gatedriverDataPublishChannel(channel, &state);

        if(printDue != FALSE)
        {
            gatedriverDataPrintChannel(channel, &state);
        }
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
