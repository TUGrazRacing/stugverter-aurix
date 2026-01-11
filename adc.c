/**********************************************************************************************************************
 * \file adc.c
 * \brief EVADC synchronized sampling triggered by GTM (Infineon-example style)
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
#define EVADC_MASTER_GROUP      ADC_GROUP_MASTER      /* you define this in adc.h (e.g. IfxEvadc_GroupId_0) */
#define EVADC_SLAVE_GROUP       ADC_GROUP_SLAVE       /* you define this in adc.h (e.g. IfxEvadc_GroupId_2) */

#define EVADC_REFILL_AND_WAIT_FOR_TRIG   ((1U << IFX_EVADC_G_Q_QINR_RF_OFF) | \
                                          (1U << IFX_EVADC_G_Q_Q0R_EXTR_OFF))

/* Infineon example: "GTM ADC trigger 3" */
#define EVADC_TRIGGER_SOURCE    IfxEvadc_TriggerSource_11
#define EVADC_TRIGGER_MODE      IfxEvadc_TriggerMode_uponRisingEdge
#define EVADC_GATING_MODE       IfxEvadc_GatingMode_always

/* ========================= Globals ========================= */
App_Adc_Type g_adc;

/* For visibility */
volatile uint16 g_measuredU = 0;
volatile uint16 g_measuredV = 0;

/* ========================= Prototypes (example style) ========================= */
IFX_STATIC void initEVADCModule(void);
IFX_STATIC void initEVADCGroupMaster(void);
IFX_STATIC void initEVADCGroupSlave(void);
IFX_STATIC void initEVADCChannelU(void);
IFX_STATIC void initEVADCChannelV(void);
IFX_STATIC void initQueuesAndStart(void);

/* ========================= ISR ========================= */
IFX_INTERRUPT(ISR_Adc_EndOfConversion, 0, ISR_PRIORITY_ADC);

void ISR_Adc_EndOfConversion(void)
{
    //flag ? logEnd() : logStart();
    //flag = !flag;
    focOpenLoop();
    Ifx_EVADC_G_RES resU = IfxEvadc_Adc_getResult(&g_adc.chnPhaseU);
    Ifx_EVADC_G_RES resV = IfxEvadc_Adc_getResult(&g_adc.chnPhaseV);

    //g_measuredU = resU.B.RESULT;
    //g_measuredV = resV.B.RESULT;
    //logTimeDiff();
}


/* ========================= Public init (call this) ========================= */
void phaseAdcInit(void)
{
    /* Debug pin */
    IfxPort_setPinModeOutput(DEBUG_PORT, DEBUG_PIN, IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);
    IfxPort_setPinLow(DEBUG_PORT, DEBUG_PIN);

    /* Example-faithful init order */
    initEVADCModule();
    initEVADCGroupMaster();
    initEVADCGroupSlave();
    initEVADCChannelU();
    initEVADCChannelV();
    initQueuesAndStart();
}

/* ========================= Module init ========================= */
IFX_STATIC void initEVADCModule(void)
{
    IfxEvadc_Adc_Config adcConfig;
    IfxEvadc_Adc_initModuleConfig(&adcConfig, &MODULE_EVADC);
    IfxEvadc_Adc_initModule(&g_adc.evadc, &adcConfig);
}

/* ========================= Group init: Master ========================= */
IFX_STATIC void initEVADCGroupMaster(void)
{
    IfxEvadc_Adc_GroupConfig groupConfig;
    IfxEvadc_Adc_initGroupConfig(&groupConfig, &g_adc.evadc);

    groupConfig.groupId = EVADC_MASTER_GROUP;
    groupConfig.master  = EVADC_MASTER_GROUP;

    /* Example uses converter OFF at init, enabled later */
    groupConfig.analogConverterMode = IfxEvadc_AnalogConverterMode_off;
    groupConfig.analogFrequency        = IFXEVADC_ANALOG_FREQUENCY_MAX;
    groupConfig.disablePostCalibration  = TRUE;
    groupConfig.startupCalibration      = FALSE;


    /* Enable Queue0 */
    groupConfig.arbiter.requestSlotQueue0Enabled = TRUE;

    /* Example config: priority + start mode */
    groupConfig.queueRequest[0].requestSlotPrio      = IfxEvadc_RequestSlotPriority_highest;
    groupConfig.queueRequest[0].requestSlotStartMode = IfxEvadc_RequestSlotStartMode_cancelInjectRepeat;

    /* Trigger config (example) */
    groupConfig.queueRequest[0].triggerConfig.gatingMode   = EVADC_GATING_MODE;
    groupConfig.queueRequest[0].triggerConfig.triggerMode  = EVADC_TRIGGER_MODE;
    groupConfig.queueRequest[0].triggerConfig.triggerSource= EVADC_TRIGGER_SOURCE;

    IfxEvadc_Adc_initGroup(&g_adc.groupMaster, &groupConfig);
}

/* ========================= Group init: Slave ========================= */
IFX_STATIC void initEVADCGroupSlave(void)
{
    IfxEvadc_Adc_GroupConfig groupConfig;
    IfxEvadc_Adc_initGroupConfig(&groupConfig, &g_adc.evadc);

    groupConfig.groupId = EVADC_SLAVE_GROUP;
    groupConfig.master  = EVADC_MASTER_GROUP;

    groupConfig.analogConverterMode = IfxEvadc_AnalogConverterMode_off;

    /* In the Infineon example, slaves are initialized without configuring trigger/queue */
    /* (We still keep default config; master provides synchronization) */
    IfxEvadc_Adc_initGroup(&g_adc.groupSlave, &groupConfig);
}

/* ========================= Channel U (Master) ========================= */
IFX_STATIC void initEVADCChannelU(void)
{
    IfxEvadc_Adc_ChannelConfig chConfig;
    IfxEvadc_Adc_initChannelConfig(&chConfig, &g_adc.groupMaster);

    chConfig.channelId        = ADC_CHN_PHASE_U;
    chConfig.resultRegister   = IfxEvadc_ChannelResult_0;
    chConfig.rightAlignedStorage = FALSE;

    /* Interrupt on master channel */
    chConfig.resultServProvider = ISR_PROVIDER_ADC;
    chConfig.resultPriority     = ISR_PRIORITY_ADC;
    chConfig.resultSrcNr        = IfxEvadc_SrcNr_group0;

    /* Example: synchronize enabled on master channel */
    chConfig.synchonize = TRUE;

    IfxEvadc_Adc_initChannel(&g_adc.chnPhaseU, &chConfig);
}

/* ========================= Channel V (Slave) ========================= */
IFX_STATIC void initEVADCChannelV(void)
{
    IfxEvadc_Adc_ChannelConfig chConfig;
    IfxEvadc_Adc_initChannelConfig(&chConfig, &g_adc.groupSlave);

    chConfig.channelId        = ADC_CHN_PHASE_V;
    chConfig.resultRegister   = IfxEvadc_ChannelResult_0;
    chConfig.rightAlignedStorage = FALSE;

    /* No interrupt from slave channel (we read it in master ISR) */
    chConfig.resultPriority = 0;

    /* Keep default sync (example does not set sync here) */
    /* If you later need stricter sync behavior, we can set synchonize=TRUE here too */

    IfxEvadc_Adc_initChannel(&g_adc.chnPhaseV, &chConfig);
}

/* ========================= Queue + start (example style) ========================= */
IFX_STATIC void initQueuesAndStart(void)
{
    /* Example only adds the MASTER channel to queue0 with refill + wait-for-trigger */
    IfxEvadc_Adc_addToQueue(&g_adc.chnPhaseU,
                            IfxEvadc_RequestSource_queue0,
                            EVADC_REFILL_AND_WAIT_FOR_TRIG);

    /* Enable analog converter (example does master only here) */
    IfxEvadc_Adc_setAnalogConvertControl(&MODULE_EVADC,
                                         &g_adc.groupMaster,
                                         IfxEvadc_AnalogConverterMode_normalOperation);

    /* Enable slave analog converter too (helps ensure it’s awake when synced) */
    IfxEvadc_Adc_setAnalogConvertControl(&MODULE_EVADC,
                                         &g_adc.groupSlave,
                                         IfxEvadc_AnalogConverterMode_normalOperation);

    /* Settling time (example uses ~3us) */
    IfxStm_waitTicks(&MODULE_STM0, (uint32)(3.0e-6f * IfxStm_getFrequency(&MODULE_STM0)));
}
