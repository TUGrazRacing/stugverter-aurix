#include "gate_driver.h"
#include <gtm/gtm.h>
#include "app_config.h"
#include <Port/Io/IfxPort_Io.h>
#include "IfxPort_Pinmap.h"
#include <math.h>

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

#define GATEDRIVER_ADC_FULL_SCALE_STEPS (4095.0f)
#define GATEDRIVER_ADC_FULL_SCALE_V     (3.5f)
#define GATEDRIVER_NTC_SUPPLY_V         (15.0f)
#define GATEDRIVER_NTC_PULLUP_OHM       (2700.0f)
#define GATEDRIVER_NTC_PARALLEL_OHM     (560.0f)
#define GATEDRIVER_NTC_R25_OHM          (5000.0f)
#define GATEDRIVER_NTC_BETA_K           (3433.0f)
#define GATEDRIVER_NTC_T25_K            (298.15f)
#define GATEDRIVER_TEMP_INVALID_DEGC    (-273.15f)

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

#if APP_GATE_DRIVER_DATA_CHANNEL_COUNT != GTM_DRIVER_DATA_CHANNEL_COUNT
#error "App gate-driver DATA channel count must match the GTM TIM channel count."
#endif

static float32 gatedriverDataCalculateNtcTemperature(uint16 adc);
static void gatedriverDataPublishHighSideTemperature(uint32 channel, const GtmDriverDataChannelState *state);
static void gatedriverDataPublishChannel(uint32 channel, const GtmDriverDataChannelState *state);
static void gatedriverDataPoll(void);
static void gatedriverDataResetPublishedState(void);

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
 * @brief Initializes GTM TIM capture and clears the published DATA readout state.
 */
void gatedriverDataServiceInit(void)
{
    gatedriverDataResetPublishedState();
    gtmDriverDataTimInit();
}

/**
 * @brief Polls all configured DATA channels and publishes their latest values.
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
 * @brief Converts a high-side driver ADC code into NTC temperature in degree Celsius.
 *
 * The ADC reports VAIP against a 3.5 V ideal full-scale. The external circuit is
 * VCC2 -> 2.7 kOhm -> VAIP -> (NTC || 560 Ohm) -> GND2.
 */
static float32 gatedriverDataCalculateNtcTemperature(uint16 adc)
{
    float32 vaip;
    float32 rBottom;
    float32 rNtc;
    float32 temperatureK;

    if(adc == 0U)
    {
        return GATEDRIVER_TEMP_INVALID_DEGC;
    }

    vaip = ((float32)adc * GATEDRIVER_ADC_FULL_SCALE_V) / GATEDRIVER_ADC_FULL_SCALE_STEPS;
    if((vaip <= 0.0f) || (vaip >= GATEDRIVER_NTC_SUPPLY_V))
    {
        return GATEDRIVER_TEMP_INVALID_DEGC;
    }

    rBottom = (GATEDRIVER_NTC_PULLUP_OHM * vaip) / (GATEDRIVER_NTC_SUPPLY_V - vaip);
    if((rBottom <= 0.0f) || (rBottom >= GATEDRIVER_NTC_PARALLEL_OHM))
    {
        return GATEDRIVER_TEMP_INVALID_DEGC;
    }

    rNtc = (rBottom * GATEDRIVER_NTC_PARALLEL_OHM) /
           (GATEDRIVER_NTC_PARALLEL_OHM - rBottom);
    if(rNtc <= 0.0f)
    {
        return GATEDRIVER_TEMP_INVALID_DEGC;
    }

    temperatureK = 1.0f / ((1.0f / GATEDRIVER_NTC_T25_K) +
                           (logf(rNtc / GATEDRIVER_NTC_R25_OHM) / GATEDRIVER_NTC_BETA_K));

    return temperatureK - 273.15f;
}

/**
 * @brief Publishes calculated SiC module temperatures from the high-side drivers.
 */
static void gatedriverDataPublishHighSideTemperature(uint32 channel, const GtmDriverDataChannelState *state)
{
    float32 temperature = GATEDRIVER_TEMP_INVALID_DEGC;

    if(state->readout.adcValid != FALSE)
    {
        temperature = gatedriverDataCalculateNtcTemperature(state->readout.adc);
    }

    switch((GtmDriverDataChannelId)channel)
    {
        case GtmDriverDataChannel_uHigh:
            app_state.gate_driver_data.sic_temperature.u_degC = temperature;
            break;

        case GtmDriverDataChannel_vHigh:
            app_state.gate_driver_data.sic_temperature.v_degC = temperature;
            break;

        case GtmDriverDataChannel_wHigh:
            app_state.gate_driver_data.sic_temperature.w_degC = temperature;
            break;

        default:
            break;
    }
}

/**
 * @brief Copies the last valid DATA readout and derived values into app_state.
 */
static void gatedriverDataPublishChannel(uint32 channel, const GtmDriverDataChannelState *state)
{
    GateDriverDataReadoutState *published = &app_state.gate_driver_data.channel[channel];

    published->adc = state->readout.adc;
    published->diagnosticFrame0 = state->readout.diagnosticFrame0;
    published->diagnosticFrame1 = state->readout.diagnosticFrame1;

    gatedriverDataPublishHighSideTemperature(channel, state);
}

/**
 * @brief Polls TIM and publishes all active channels for Ethernet logging.
 */
static void gatedriverDataPoll(void)
{
    uint32 channel;

    (void)gtmDriverDataTimProcess();

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
    }
}

/**
 * @brief Clears published DATA values and marks derived temperatures invalid.
 */
static void gatedriverDataResetPublishedState(void)
{
    uint32 channel;

    for(channel = 0U; channel < APP_GATE_DRIVER_DATA_CHANNEL_COUNT; channel++)
    {
        app_state.gate_driver_data.channel[channel].adc = 0U;
        app_state.gate_driver_data.channel[channel].diagnosticFrame0 = 0U;
        app_state.gate_driver_data.channel[channel].diagnosticFrame1 = 0U;
    }

    app_state.gate_driver_data.sic_temperature.u_degC = GATEDRIVER_TEMP_INVALID_DEGC;
    app_state.gate_driver_data.sic_temperature.v_degC = GATEDRIVER_TEMP_INVALID_DEGC;
    app_state.gate_driver_data.sic_temperature.w_degC = GATEDRIVER_TEMP_INVALID_DEGC;
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
