#include "gtm_tim.h"

#include "gtm.h"
#include "IfxGtm_PinMap.h"
#include "IfxGtm_Tim_In.h"
#include "IfxPort.h"

#define GATEDRIVER_DATA_PWM_SCALE        (8192U)
#define GATEDRIVER_DATA_ADC_MASK         (0x0FFFU)
#define GATEDRIVER_DATA_DIAG_FRAME_MASK  (0x1FFFU)
#define GATEDRIVER_DATA_DIAG_BIT         (0x1000U)
#define GATEDRIVER_DATA_DIAG1_BIT        (0x0800U)

#define GATEDRIVER_DATA_U_LOW_PWM_IN     (&IfxGtm_TIM0_0_P00_9_IN)
#define GATEDRIVER_DATA_V_LOW_PWM_IN     (NULL_PTR)
#define GATEDRIVER_DATA_W_LOW_PWM_IN     (NULL_PTR)
#define GATEDRIVER_DATA_U_HIGH_PWM_IN    (NULL_PTR)
#define GATEDRIVER_DATA_V_HIGH_PWM_IN    (NULL_PTR)
#define GATEDRIVER_DATA_W_HIGH_PWM_IN    (NULL_PTR)

typedef struct
{
    IfxGtm_Tim_TinMap *pin;
    IfxPort_InputMode pinMode;
} GtmDriverDataChannelConfig;

typedef struct
{
    IfxGtm_Tim_In driver;
    GtmDriverDataChannelState state;
} GtmDriverDataTimChannel;

static const GtmDriverDataChannelConfig g_driverDataConfig[GTM_DRIVER_DATA_CHANNEL_COUNT] =
{
    {GATEDRIVER_DATA_U_LOW_PWM_IN,  IfxPort_InputMode_noPullDevice},
    {GATEDRIVER_DATA_V_LOW_PWM_IN,  IfxPort_InputMode_noPullDevice},
    {GATEDRIVER_DATA_W_LOW_PWM_IN,  IfxPort_InputMode_noPullDevice},
    {GATEDRIVER_DATA_U_HIGH_PWM_IN, IfxPort_InputMode_noPullDevice},
    {GATEDRIVER_DATA_V_HIGH_PWM_IN, IfxPort_InputMode_noPullDevice},
    {GATEDRIVER_DATA_W_HIGH_PWM_IN, IfxPort_InputMode_noPullDevice},
};

static GtmDriverDataTimChannel g_driverDataChannels[GTM_DRIVER_DATA_CHANNEL_COUNT];
static volatile uint32 g_driverDataSequence;

float32 g_measuredPwmDutyCycle = 0.0f;
float32 g_measuredPwmFreq_Hz = 0.0f;
float32 g_measuredPwmPeriod = 0.0f;
uint32  g_measuredPwmPeriodTicks = 0U;
uint32  g_measuredPwmPulseTicks = 0U;
boolean g_dataCoherent = FALSE;
boolean g_measuredPwmNewData = FALSE;
boolean g_measuredPwmDataLost = FALSE;
boolean g_measuredPwmOverflow = FALSE;
boolean g_measuredPwmGlitch = FALSE;

static boolean gtmDriverDataDecodeFrame(GtmDriverDataChannelState *channel);
static void gtmDriverDataPublishLegacyDebug(void);
static void gtmDriverDataResetChannel(GtmDriverDataChannelId channelId);

void gtmDriverDataTimInit(void)
{
    uint32 channel;

    gtmEnableClock0();

    g_driverDataSequence++;

    for(channel = 0U; channel < GTM_DRIVER_DATA_CHANNEL_COUNT; channel++)
    {
        gtmDriverDataResetChannel((GtmDriverDataChannelId)channel);

        if(g_driverDataConfig[channel].pin != NULL_PTR)
        {
            IfxGtm_Tim_In_Config configTIM;

            IfxGtm_Tim_In_initConfig(&configTIM, &MODULE_GTM);
            configTIM.filter.inputPin = g_driverDataConfig[channel].pin;
            configTIM.filter.inputPinMode = g_driverDataConfig[channel].pinMode;
            configTIM.capture.clock = IfxGtm_Cmu_Clk_0;
            configTIM.capture.mode = Ifx_Pwm_Mode_leftAligned;
            configTIM.mode = IfxGtm_Tim_Mode_pwmMeasurement;

            g_driverDataChannels[channel].state.enabled = TRUE;
            g_driverDataChannels[channel].state.pwm.initialized =
                IfxGtm_Tim_In_init(&g_driverDataChannels[channel].driver, &configTIM);
        }
    }

    gtmDriverDataPublishLegacyDebug();
    g_driverDataSequence++;
}

boolean gtmDriverDataTimProcess(void)
{
    boolean anyUpdated = FALSE;
    uint32 channel;

    g_driverDataSequence++;

    for(channel = 0U; channel < GTM_DRIVER_DATA_CHANNEL_COUNT; channel++)
    {
        GtmDriverDataTimChannel *timChannel = &g_driverDataChannels[channel];
        GtmDriverDataChannelState *state = &timChannel->state;

        if((state->enabled == FALSE) || (state->pwm.initialized == FALSE))
        {
            continue;
        }

        IfxGtm_Tim_In_update(&timChannel->driver);

        state->pwm.newData = IfxGtm_Tim_In_isNewData(&timChannel->driver);
        state->pwm.dataLost = IfxGtm_Tim_In_isDataLost(&timChannel->driver);
        state->pwm.overflow = timChannel->driver.overflowCnt;
        state->pwm.glitch = timChannel->driver.glitch;

        if(state->pwm.dataLost != FALSE)
        {
            state->dataLostCount++;
        }

        if(state->pwm.overflow != FALSE)
        {
            state->overflowCount++;
        }

        if(state->pwm.glitch != FALSE)
        {
            state->glitchCount++;
        }

        if(state->pwm.newData != FALSE)
        {
            state->pwm.periodTicks = (uint32)IfxGtm_Tim_In_getPeriodTicks(&timChannel->driver);
            state->pwm.pulseTicks = (uint32)IfxGtm_Tim_In_getPulseLengthTick(&timChannel->driver);
            state->pwm.dataCoherent = timChannel->driver.dataCoherent;

            if((state->pwm.periodTicks > 0U) &&
               (state->pwm.dataCoherent != FALSE) &&
               (state->pwm.dataLost == FALSE) &&
               (state->pwm.overflow == FALSE))
            {
                state->pwm.periodSeconds = IfxGtm_Tim_In_getPeriodSecond(&timChannel->driver);

                if(state->pwm.periodSeconds > 0.0f)
                {
                    state->pwm.frequencyHz = 1.0f / state->pwm.periodSeconds;
                    state->pwm.dutyPercent =
                        ((float32)state->pwm.pulseTicks * 100.0f) /
                        (float32)state->pwm.periodTicks;
                    if(gtmDriverDataDecodeFrame(state) != FALSE)
                    {
                        anyUpdated = TRUE;
                    }
                }
                else
                {
                    state->invalidFrameCount++;
                }
            }
            else
            {
                state->invalidFrameCount++;
            }
        }
    }

    gtmDriverDataPublishLegacyDebug();
    g_driverDataSequence++;

    return anyUpdated;
}

boolean gtmDriverDataTimUpdate(void)
{
    return gtmDriverDataTimProcess();
}

boolean gtmDriverDataTimCopyChannel(GtmDriverDataChannelId channelId, GtmDriverDataChannelState *channel)
{
    uint32 startSequence;
    uint32 endSequence;

    if((channelId >= GtmDriverDataChannel_count) || (channel == NULL_PTR))
    {
        return FALSE;
    }

    do
    {
        startSequence = g_driverDataSequence;
        *channel = g_driverDataChannels[channelId].state;
        endSequence = g_driverDataSequence;
    } while((startSequence != endSequence) || ((endSequence & 1U) != 0U));

    return channel->enabled;
}

const GtmDriverDataChannelState *gtmDriverDataTimGetChannel(GtmDriverDataChannelId channelId)
{
    if(channelId >= GtmDriverDataChannel_count)
    {
        return NULL_PTR;
    }

    return &g_driverDataChannels[channelId].state;
}

const GtmPwmInputMeasurement *gtmDriverDataTimGetMeasurement(void)
{
    return &g_driverDataChannels[GtmDriverDataChannel_uLow].state.pwm;
}

const GtmDriverDataReadout *gtmDriverDataGetReadout(void)
{
    return &g_driverDataChannels[GtmDriverDataChannel_uLow].state.readout;
}

uint32 gtmDriverDataTimGetSequence(void)
{
    return g_driverDataSequence;
}

void initTIM(void)
{
    gtmDriverDataTimInit();
}

void measure_PWM(void)
{
    (void)gtmDriverDataTimProcess();
}

static void gtmDriverDataResetChannel(GtmDriverDataChannelId channelId)
{
    GtmDriverDataChannelState *state = &g_driverDataChannels[channelId].state;

    state->channelId = channelId;
    state->enabled = FALSE;
    state->pwm.dutyPercent = 0.0f;
    state->pwm.frequencyHz = 0.0f;
    state->pwm.periodSeconds = 0.0f;
    state->pwm.periodTicks = 0U;
    state->pwm.pulseTicks = 0U;
    state->pwm.dataCoherent = FALSE;
    state->pwm.newData = FALSE;
    state->pwm.dataLost = FALSE;
    state->pwm.overflow = FALSE;
    state->pwm.glitch = FALSE;
    state->pwm.initialized = FALSE;
    state->readout.adc = 0U;
    state->readout.diagnosticFrame0 = 0U;
    state->readout.diagnosticFrame1 = 0U;
    state->readout.lastRawFrame = 0U;
    state->readout.lastFrameType = GtmDriverDataFrameType_unknown;
    state->readout.adcValid = FALSE;
    state->readout.diagnosticFrame0Valid = FALSE;
    state->readout.diagnosticFrame1Valid = FALSE;
    state->frameCount = 0U;
    state->invalidFrameCount = 0U;
    state->dataLostCount = 0U;
    state->overflowCount = 0U;
    state->glitchCount = 0U;
}

static boolean gtmDriverDataDecodeFrame(GtmDriverDataChannelState *channel)
{
    uint32 rawFrame;

    rawFrame = (uint32)((((uint64)channel->pwm.pulseTicks * GATEDRIVER_DATA_PWM_SCALE) +
                         ((uint64)channel->pwm.periodTicks / 2ULL)) /
                        (uint64)channel->pwm.periodTicks);

    if(rawFrame > GATEDRIVER_DATA_DIAG_FRAME_MASK)
    {
        rawFrame = GATEDRIVER_DATA_DIAG_FRAME_MASK;
    }

    channel->readout.lastRawFrame = (uint16)rawFrame;

    if(rawFrame == 0U)
    {
        channel->readout.lastFrameType = GtmDriverDataFrameType_unknown;
        channel->invalidFrameCount++;
        return FALSE;
    }

    channel->frameCount++;

    if((rawFrame & GATEDRIVER_DATA_DIAG_BIT) == 0U)
    {
        channel->readout.adc = (uint16)(rawFrame & GATEDRIVER_DATA_ADC_MASK);
        channel->readout.adcValid = TRUE;
        channel->readout.lastFrameType = GtmDriverDataFrameType_adc;
    }
    else if((rawFrame & GATEDRIVER_DATA_DIAG1_BIT) == 0U)
    {
        channel->readout.diagnosticFrame0 = (uint16)rawFrame;
        channel->readout.diagnosticFrame0Valid = TRUE;
        channel->readout.lastFrameType = GtmDriverDataFrameType_diag0;
    }
    else
    {
        channel->readout.diagnosticFrame1 = (uint16)rawFrame;
        channel->readout.diagnosticFrame1Valid = TRUE;
        channel->readout.lastFrameType = GtmDriverDataFrameType_diag1;
    }

    return TRUE;
}

static void gtmDriverDataPublishLegacyDebug(void)
{
    const GtmPwmInputMeasurement *measurement = &g_driverDataChannels[GtmDriverDataChannel_uLow].state.pwm;

    g_measuredPwmDutyCycle = measurement->dutyPercent;
    g_measuredPwmFreq_Hz = measurement->frequencyHz;
    g_measuredPwmPeriod = measurement->periodSeconds;
    g_measuredPwmPeriodTicks = measurement->periodTicks;
    g_measuredPwmPulseTicks = measurement->pulseTicks;
    g_dataCoherent = measurement->dataCoherent;
    g_measuredPwmNewData = measurement->newData;
    g_measuredPwmDataLost = measurement->dataLost;
    g_measuredPwmOverflow = measurement->overflow;
    g_measuredPwmGlitch = measurement->glitch;
}
