/**********************************************************************************************************************
 * \file adc.c
 * \brief EVADC synchronized sampling triggered by GTM:
 *        Group0 dummy master triggers + ISR, real measurements in two slave groups in parallel.
 *********************************************************************************************************************/

#include "adc.h"
#include "IfxEvadc_Adc.h"
#include "IfxPort.h"
#include "IfxStm.h"
#include "logger.h"
#include "foc.h"

/* ========================= User debug pin ========================= */
#define DEBUG_PORT   (&MODULE_P14)
#define DEBUG_PIN    (5)

/* ========================= Example-faithful macros ========================= */
#define EVADC_MASTER_GROUP              ADC_GROUP_DUMMY_MASTER
#define EVADC_SLAVE_GROUP_A             ADC_GROUP_SLAVE_A
#define EVADC_SLAVE_GROUP_B             ADC_GROUP_SLAVE_B

#define EVADC_REFILL_AND_WAIT_FOR_TRIG  ((1U << IFX_EVADC_G_Q_QINR_RF_OFF) | \
                                         (1U << IFX_EVADC_G_Q_Q0R_EXTR_OFF))

/* Infineon example: "GTM ADC trigger 3" (keep as you had) */
#define EVADC_TRIGGER_SOURCE            IfxEvadc_TriggerSource_11
#define EVADC_TRIGGER_MODE              IfxEvadc_TriggerMode_uponRisingEdge
#define EVADC_GATING_MODE               IfxEvadc_GatingMode_always

/* ========================= Globals ========================= */
App_Adc_Type g_adc;

/* For visibility */
volatile uint16 g_measuredA = 0;
volatile uint16 g_measuredB = 0;

/* ========================= Prototypes (example style) ========================= */
IFX_STATIC void initEVADCModule(void);
IFX_STATIC void initEVADCDummyMasterGroup(void);
IFX_STATIC void initEVADCSlaveGroupA(void);
IFX_STATIC void initEVADCSlaveGroupB(void);

IFX_STATIC void initEVADCDummyChannel(void);
IFX_STATIC void initEVADCChannelPhaseA(void);
IFX_STATIC void initEVADCChannelPhaseB(void);

IFX_STATIC void initQueuesAndStart(void);

/* ========================= ISR ========================= */
IFX_INTERRUPT(ISR_Adc_EndOfConversion, 0, ISR_PRIORITY_ADC);

void ISR_Adc_EndOfConversion(void)
{
    /* Optional: scope timing */
    IfxPort_setPinHigh(DEBUG_PORT, DEBUG_PIN);

    focOpenLoop();

    /* Read the real measurements from the two slave channels */
    Ifx_EVADC_G_RES resA = IfxEvadc_Adc_getResult(&g_adc.chnPhaseA);
    Ifx_EVADC_G_RES resB = IfxEvadc_Adc_getResult(&g_adc.chnPhaseB);

    g_measuredA = (uint16)resA.B.RESULT;
    g_measuredB = (uint16)resB.B.RESULT;

    /* Push to logger (Non-blocking) */
    logPush(g_measuredA, g_measuredB);

    IfxPort_setPinLow(DEBUG_PORT, DEBUG_PIN);
}

/* ========================= Public init (call this) ========================= */
void phaseAdcInit(void)
{
    /* Debug pin */
    IfxPort_setPinModeOutput(DEBUG_PORT, DEBUG_PIN, IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);
    IfxPort_setPinLow(DEBUG_PORT, DEBUG_PIN);

    /* Example-faithful init order */
    initEVADCModule();

    /* Groups */
    initEVADCDummyMasterGroup();
    initEVADCSlaveGroupA();
    initEVADCSlaveGroupB();

    /* Channels */
    initEVADCDummyChannel();
    initEVADCChannelPhaseA();
    initEVADCChannelPhaseB();

    /* Queue + enable converters */
    initQueuesAndStart();
}

/* ========================= Module init ========================= */
IFX_STATIC void initEVADCModule(void)
{
    IfxEvadc_Adc_Config adcConfig;
    IfxEvadc_Adc_initModuleConfig(&adcConfig, &MODULE_EVADC);
    IfxEvadc_Adc_initModule(&g_adc.evadc, &adcConfig);
}

/* ========================= Group init: Dummy Master (Group0) ========================= */
IFX_STATIC void initEVADCDummyMasterGroup(void)
{
    IfxEvadc_Adc_GroupConfig groupConfig;
    IfxEvadc_Adc_initGroupConfig(&groupConfig, &g_adc.evadc);

    groupConfig.groupId = EVADC_MASTER_GROUP;
    groupConfig.master  = EVADC_MASTER_GROUP;

    /* Example uses converter OFF at init, enabled later */
    groupConfig.analogConverterMode    = IfxEvadc_AnalogConverterMode_off;
    groupConfig.analogFrequency        = IFXEVADC_ANALOG_FREQUENCY_MAX;
    groupConfig.disablePostCalibration = TRUE;
    groupConfig.startupCalibration     = FALSE;

    /* Enable Queue0 */
    groupConfig.arbiter.requestSlotQueue0Enabled = TRUE;

    /* Example config: priority + start mode */
    groupConfig.queueRequest[0].requestSlotPrio      = IfxEvadc_RequestSlotPriority_highest;
    groupConfig.queueRequest[0].requestSlotStartMode = IfxEvadc_RequestSlotStartMode_cancelInjectRepeat;

    /* Trigger config (example) */
    groupConfig.queueRequest[0].triggerConfig.gatingMode    = EVADC_GATING_MODE;
    groupConfig.queueRequest[0].triggerConfig.triggerMode   = EVADC_TRIGGER_MODE;
    groupConfig.queueRequest[0].triggerConfig.triggerSource = EVADC_TRIGGER_SOURCE;

    IfxEvadc_Adc_initGroup(&g_adc.groupDummy, &groupConfig);
}

/* ========================= Group init: Slave A ========================= */
IFX_STATIC void initEVADCSlaveGroupA(void)
{
    IfxEvadc_Adc_GroupConfig groupConfig;
    IfxEvadc_Adc_initGroupConfig(&groupConfig, &g_adc.evadc);

    groupConfig.groupId = EVADC_SLAVE_GROUP_A;
    groupConfig.master  = EVADC_MASTER_GROUP;

    groupConfig.analogConverterMode    = IfxEvadc_AnalogConverterMode_off;
    groupConfig.analogFrequency        = IFXEVADC_ANALOG_FREQUENCY_MAX;
    groupConfig.disablePostCalibration = TRUE;
    groupConfig.startupCalibration     = FALSE;

    /* Slaves: do not configure queue/trigger here (master provides synchronization) */
    IfxEvadc_Adc_initGroup(&g_adc.groupSlaveA, &groupConfig);
}

/* ========================= Group init: Slave B ========================= */
IFX_STATIC void initEVADCSlaveGroupB(void)
{
    IfxEvadc_Adc_GroupConfig groupConfig;
    IfxEvadc_Adc_initGroupConfig(&groupConfig, &g_adc.evadc);

    groupConfig.groupId = EVADC_SLAVE_GROUP_B;
    groupConfig.master  = EVADC_MASTER_GROUP;

    groupConfig.analogConverterMode    = IfxEvadc_AnalogConverterMode_off;
    groupConfig.analogFrequency        = IFXEVADC_ANALOG_FREQUENCY_MAX;
    groupConfig.disablePostCalibration = TRUE;
    groupConfig.startupCalibration     = FALSE;

    /* Slaves: do not configure queue/trigger here (master provides synchronization) */
    IfxEvadc_Adc_initGroup(&g_adc.groupSlaveB, &groupConfig);
}

/* ========================= Dummy Channel (Master ISR source) ========================= */
IFX_STATIC void initEVADCDummyChannel(void)
{
    IfxEvadc_Adc_ChannelConfig chConfig;
    IfxEvadc_Adc_initChannelConfig(&chConfig, &g_adc.groupDummy);

    chConfig.channelId           = ADC_CHN_DUMMY;
    chConfig.resultRegister      = IfxEvadc_ChannelResult_0;
    chConfig.rightAlignedStorage = FALSE;

    /* Interrupt on dummy master channel */
    chConfig.resultServProvider = ISR_PROVIDER_ADC;
    chConfig.resultPriority     = ISR_PRIORITY_ADC;
    chConfig.resultSrcNr        = IfxEvadc_SrcNr_group0;

    /* Enable synchronization on the master trigger channel */
    chConfig.synchonize = TRUE;

    IfxEvadc_Adc_initChannel(&g_adc.chnDummy, &chConfig);
}

/* ========================= Phase A Channel (Slave A) ========================= */
IFX_STATIC void initEVADCChannelPhaseA(void)
{
    IfxEvadc_Adc_ChannelConfig chConfig;
    IfxEvadc_Adc_initChannelConfig(&chConfig, &g_adc.groupSlaveA);

    chConfig.channelId           = ADC_CHN_PHASE_U;
    chConfig.resultRegister      = IfxEvadc_ChannelResult_0;
    chConfig.rightAlignedStorage = FALSE;

    /* No interrupt from slave channel */
    chConfig.resultPriority = 0;

    /* Participate in synchronized conversion */
    chConfig.synchonize = TRUE;

    IfxEvadc_Adc_initChannel(&g_adc.chnPhaseA, &chConfig);
}

/* ========================= Phase B Channel (Slave B) ========================= */
IFX_STATIC void initEVADCChannelPhaseB(void)
{
    IfxEvadc_Adc_ChannelConfig chConfig;
    IfxEvadc_Adc_initChannelConfig(&chConfig, &g_adc.groupSlaveB);

    chConfig.channelId           = ADC_CHN_PHASE_V;
    chConfig.resultRegister      = IfxEvadc_ChannelResult_0;
    chConfig.rightAlignedStorage = FALSE;

    /* No interrupt from slave channel */
    chConfig.resultPriority = 0;

    /* Participate in synchronized conversion */
    chConfig.synchonize = TRUE;

    IfxEvadc_Adc_initChannel(&g_adc.chnPhaseB, &chConfig);
}

/* ========================= Queue + start (example style) ========================= */
IFX_STATIC void initQueuesAndStart(void)
{
    /* Only add the DUMMY MASTER channel to queue0 with refill + wait-for-trigger */
    IfxEvadc_Adc_addToQueue(&g_adc.chnDummy,
                            IfxEvadc_RequestSource_queue0,
                            EVADC_REFILL_AND_WAIT_FOR_TRIG);

    /* Enable analog converter for all groups so slaves are awake and can sync */
    IfxEvadc_Adc_setAnalogConvertControl(&MODULE_EVADC,
                                         &g_adc.groupDummy,
                                         IfxEvadc_AnalogConverterMode_normalOperation);

    IfxEvadc_Adc_setAnalogConvertControl(&MODULE_EVADC,
                                         &g_adc.groupSlaveA,
                                         IfxEvadc_AnalogConverterMode_normalOperation);

    IfxEvadc_Adc_setAnalogConvertControl(&MODULE_EVADC,
                                         &g_adc.groupSlaveB,
                                         IfxEvadc_AnalogConverterMode_normalOperation);

    /* Settling time (example uses ~3us) */
    IfxStm_waitTicks(&MODULE_STM0, (uint32)(3.0e-6f * IfxStm_getFrequency(&MODULE_STM0)));
}
