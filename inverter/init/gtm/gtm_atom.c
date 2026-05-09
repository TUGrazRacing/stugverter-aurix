#include "gtm_atom.h"

#include "gtm.h"
#include "IfxGtm_Atom.h"
#include "IfxGtm_PinMap.h"
#include "IfxGtm_Pwm.h"
#include "IfxGtm_Trig.h"
#include "IfxPort.h"

#define NUM_OF_CHANNELS         (3)
#define ISR_PRIORITY_ATOM       (20)

#define PHASE_U_HS              &IfxGtm_ATOM1_0_TOUT0_P02_0_OUT
#define PHASE_U_LS              &IfxGtm_ATOM1_0N_TOUT7_P02_7_OUT
#define PHASE_V_HS              &IfxGtm_ATOM1_1_TOUT1_P02_1_OUT
#define PHASE_V_LS              &IfxGtm_ATOM1_1N_TOUT4_P02_4_OUT
#define PHASE_W_HS              &IfxGtm_ATOM1_2_TOUT2_P02_2_OUT
#define PHASE_W_LS              &IfxGtm_ATOM1_2N_TOUT5_P02_5_OUT
#define TRG_OUT                 &IfxGtm_ATOM1_3_TOUT3_P02_3_OUT
#define TRG_CH                  IfxGtm_Atom_Ch_3

#define TRIGGER_OFFSET          (-50)

#define PHASE_U_DUTY            (50.0f)
#define PHASE_V_DUTY            (50.0f)
#define PHASE_W_DUTY            (50.0f)

typedef struct
{
    IfxGtm_Pwm          pwm;
    IfxGtm_Pwm_Channel  channels[NUM_OF_CHANNELS];
    float32             dutyCycles[NUM_OF_CHANNELS];
    IfxGtm_Pwm_DeadTime deadTimes[NUM_OF_CHANNELS];
} GtmAtom3phInv;

IFX_STATIC GtmAtom3phInv g_gtmAtom3phInv;

PwmConfig *pwm_config;

IFX_INTERRUPT(interruptGtmAtom, 0, ISR_PRIORITY_ATOM);

void interruptGtmAtom(void)
{
}

void IfxGtm_periodEventFunction(void *data)
{
    (void)data;
}

static void pwmInitAdcTriggerChannel(void)
{
    Ifx_GTM_ATOM *atom = &MODULE_GTM.ATOM[IfxGtm_Atom_1];
    Ifx_GTM_ATOM_AGC *agc = &atom->AGC;
    uint32 fullPeriodTicks = (uint32)((g_gtmAtom3phInv.pwm.sourceFrequency / (float32)pwm_config->frequency) + 0.5f);
    uint32 triggerPoint = (fullPeriodTicks - 1U) + TRIGGER_OFFSET;

    IfxGtm_Atom_Ch_configurePwmMode(atom,
                                    TRG_CH,
                                    IfxGtm_Cmu_Clk_0,
                                    Ifx_ActiveState_high,
                                    IfxGtm_Atom_Ch_ResetEvent_onTrigger,
                                    IfxGtm_Atom_Ch_OutputTrigger_forward);
    IfxGtm_Atom_Ch_setCounterValue(atom, TRG_CH, 0U);
    IfxGtm_Atom_Ch_setCompare(atom, TRG_CH, fullPeriodTicks, triggerPoint + 1U);
    IfxGtm_Atom_Ch_setCompareShadow(atom, TRG_CH, fullPeriodTicks, triggerPoint + 1U);

    IfxGtm_Atom_Agc_enableChannelUpdate(agc, TRG_CH, TRUE);
    IfxGtm_Atom_Agc_enableChannel(agc, TRG_CH, TRUE, TRUE);
    IfxGtm_Atom_Agc_enableChannelOutput(agc, TRG_CH, TRUE, TRUE);
    IfxGtm_PinMap_setAtomTout((IfxGtm_Atom_ToutMap *)TRG_OUT,
                              IfxPort_OutputMode_pushPull,
                              IfxPort_PadDriver_cmosAutomotiveSpeed1);

    IfxGtm_Trig_toEVadc(&MODULE_GTM,
                        IfxGtm_Trig_AdcGroup_0,
                        IfxGtm_Trig_AdcTrig_3,
                        IfxGtm_Trig_AdcTrigSource_atom1,
                        IfxGtm_Trig_AdcTrigChannel_3);
}

void pwmInit(PwmConfig *pconfig)
{
    IfxGtm_Pwm_Config config;
    IfxGtm_Pwm_ChannelConfig channelConfig[NUM_OF_CHANNELS];
    IfxGtm_Pwm_DtmConfig dtmConfig[NUM_OF_CHANNELS];
    IfxGtm_Pwm_InterruptConfig interruptConfig;
    IfxGtm_Pwm_OutputConfig output[NUM_OF_CHANNELS];

    pwm_config = pconfig;

    IfxGtm_Pwm_initConfig(&config, &MODULE_GTM);

    output[0].pin                   = (IfxGtm_Pwm_ToutMap *)PHASE_U_HS;
    output[0].complementaryPin      = (IfxGtm_Pwm_ToutMap *)PHASE_U_LS;
    output[0].polarity              = Ifx_ActiveState_high;
    output[0].complementaryPolarity = Ifx_ActiveState_low;
    output[0].outputMode            = IfxPort_OutputMode_pushPull;
    output[0].padDriver             = IfxPort_PadDriver_cmosAutomotiveSpeed1;

    output[1].pin                   = (IfxGtm_Pwm_ToutMap *)PHASE_V_HS;
    output[1].complementaryPin      = (IfxGtm_Pwm_ToutMap *)PHASE_V_LS;
    output[1].polarity              = Ifx_ActiveState_high;
    output[1].complementaryPolarity = Ifx_ActiveState_low;
    output[1].outputMode            = IfxPort_OutputMode_pushPull;
    output[1].padDriver             = IfxPort_PadDriver_cmosAutomotiveSpeed1;

    output[2].pin                   = (IfxGtm_Pwm_ToutMap *)PHASE_W_HS;
    output[2].complementaryPin      = (IfxGtm_Pwm_ToutMap *)PHASE_W_LS;
    output[2].polarity              = Ifx_ActiveState_high;
    output[2].complementaryPolarity = Ifx_ActiveState_low;
    output[2].outputMode            = IfxPort_OutputMode_pushPull;
    output[2].padDriver             = IfxPort_PadDriver_cmosAutomotiveSpeed1;

    dtmConfig[0].deadTime.rising = 1e-6f;
    dtmConfig[0].deadTime.falling = 1e-6f;
    dtmConfig[1].deadTime.rising = 1e-6f;
    dtmConfig[1].deadTime.falling = 1e-6f;
    dtmConfig[2].deadTime.rising = 1e-6f;
    dtmConfig[2].deadTime.falling = 1e-6f;

    interruptConfig.mode        = IfxGtm_IrqMode_pulseNotify;
    interruptConfig.isrProvider = IfxSrc_Tos_cpu0;
    interruptConfig.priority    = ISR_PRIORITY_ATOM;
    interruptConfig.periodEvent = IfxGtm_periodEventFunction;
    interruptConfig.dutyEvent   = NULL_PTR;

    channelConfig[0].timerCh   = IfxGtm_Pwm_SubModule_Ch_0;
    channelConfig[0].phase     = 0.0f;
    channelConfig[0].duty      = PHASE_U_DUTY;
    channelConfig[0].dtm       = &dtmConfig[0];
    channelConfig[0].output    = &output[0];
    channelConfig[0].mscOut    = NULL_PTR;
    channelConfig[0].interrupt = &interruptConfig;

    channelConfig[1].timerCh   = IfxGtm_Pwm_SubModule_Ch_1;
    channelConfig[1].phase     = 0.0f;
    channelConfig[1].duty      = PHASE_V_DUTY;
    channelConfig[1].dtm       = &dtmConfig[1];
    channelConfig[1].output    = &output[1];
    channelConfig[1].mscOut    = NULL_PTR;
    channelConfig[1].interrupt = NULL_PTR;

    channelConfig[2].timerCh   = IfxGtm_Pwm_SubModule_Ch_2;
    channelConfig[2].phase     = 0.0f;
    channelConfig[2].duty      = PHASE_W_DUTY;
    channelConfig[2].dtm       = &dtmConfig[2];
    channelConfig[2].output    = &output[2];
    channelConfig[2].mscOut    = NULL_PTR;
    channelConfig[2].interrupt = NULL_PTR;

    config.cluster           = IfxGtm_Cluster_1;
    config.subModule         = IfxGtm_Pwm_SubModule_atom;
    config.alignment         = IfxGtm_Pwm_Alignment_center;
    config.syncStart         = TRUE;
    config.numChannels       = NUM_OF_CHANNELS;
    config.channels          = channelConfig;
    config.frequency         = (float32)pwm_config->frequency;
    config.clockSource.atom  = IfxGtm_Cmu_Clk_0;
    config.dtmClockSource    = IfxGtm_Dtm_ClockSource_cmuClock0;
    config.syncUpdateEnabled = TRUE;

    gtmEnableClock0();

    IfxGtm_Pwm_init(&g_gtmAtom3phInv.pwm, &g_gtmAtom3phInv.channels[0], &config);

    pwmInitAdcTriggerChannel();

    g_gtmAtom3phInv.dutyCycles[0] = channelConfig[0].duty;
    g_gtmAtom3phInv.dutyCycles[1] = channelConfig[1].duty;
    g_gtmAtom3phInv.dutyCycles[2] = channelConfig[2].duty;
    g_gtmAtom3phInv.deadTimes[0] = channelConfig[0].dtm->deadTime;
    g_gtmAtom3phInv.deadTimes[1] = channelConfig[1].dtm->deadTime;
    g_gtmAtom3phInv.deadTimes[2] = channelConfig[2].dtm->deadTime;
}

void setDutyCycles(float32 dutyU, float32 dutyV, float32 dutyW)
{
    (void)dutyU;
    (void)dutyV;
    (void)dutyW;

//    IfxGtm_Pwm_updateChannelDutyImmediate(&g_gtmAtom3phInv.pwm, IfxGtm_Pwm_SyncChannelIndex_0, dutyU);
//    IfxGtm_Pwm_updateChannelDutyImmediate(&g_gtmAtom3phInv.pwm, IfxGtm_Pwm_SyncChannelIndex_1, dutyV);
//    IfxGtm_Pwm_updateChannelDutyImmediate(&g_gtmAtom3phInv.pwm, IfxGtm_Pwm_SyncChannelIndex_2, dutyW);
}
