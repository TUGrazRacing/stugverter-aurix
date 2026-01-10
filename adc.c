/**********************************************************************************************************************
 * \file adc.c
 * \brief Implementation of EVADC Parallel Sampling with GTM Trigger.
 *********************************************************************************************************************/

#include "adc.h"
#include "IfxPort.h"


#define DEBUG   &MODULE_P14, 5                         /* LED which will be toggled in Interrupt Service Routine (ISR) */
/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/
App_Adc_Type g_adc;

/* Variable to store results for debug visibility */
volatile uint16 g_measuredU = 0;
volatile uint16 g_measuredV = 0;

/*********************************************************************************************************************/
/*--------------------------------------------Function Implementations-----------------------------------------------*/
/*********************************************************************************************************************/

/* Macro to define the Interrupt Service Routine */
IFX_INTERRUPT(ISR_Adc_EndOfConversion, 0, ISR_PRIORITY_ADC);

/**
 * \brief ADC End of Conversion Interrupt Service Routine.
 * This runs immediately after the PWM Center Point.
 */
void ISR_Adc_EndOfConversion(void)
{
    IfxPort_togglePin(DEBUG);                                                      /* Toggle the LED                   */
    /* 1. Read Results */
    /* Note: We read the Result Register. If valid flag is needed, use .B.VF checks.
     * With synchronized groups, Master finishes slightly after slave, so both are ready. */

    Ifx_EVADC_G_RES resultU_reg = IfxEvadc_Adc_getResult(&g_adc.chnPhaseU);
    Ifx_EVADC_G_RES resultV_reg = IfxEvadc_Adc_getResult(&g_adc.chnPhaseV);

    g_measuredU = resultU_reg.B.RESULT;
    g_measuredV = resultV_reg.B.RESULT;

    /* 2. (Optional) Run FOC / Control Loop here */

    /* 3. (Optional) Clear Interrupt Flag (Handled by SRC usually, but good practice if using bitfields) */
}

/**
 * \brief Initialize the EVADC module, groups, and channels.
 */
/* Mapping: GTM Trig 3 usually maps to Input 11 on TC3xx EVADC */

void initEvadc(void)
{
    IfxPort_setPinModeOutput(DEBUG, IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);   /* Set pin mode         */
    /* 1. Module Config */
    IfxEvadc_Adc_Config adcConfig;
    IfxEvadc_Adc_initModuleConfig(&adcConfig, &MODULE_EVADC);
    IfxEvadc_Adc_initModule(&g_adc.evadc, &adcConfig);

    /* 2. Group Config */
    IfxEvadc_Adc_GroupConfig groupConfig;
    IfxEvadc_Adc_initGroupConfig(&groupConfig, &g_adc.evadc);

    /* ---------------- MASTER GROUP (Phase U) ---------------- */
    groupConfig.groupId = ADC_GROUP_MASTER;
    groupConfig.master  = ADC_GROUP_MASTER;

    // Enable Queue 0
    groupConfig.arbiter.requestSlotQueue0Enabled = TRUE;

    // Configure Hardware Trigger
    groupConfig.queueRequest[0].triggerConfig.gatingMode = IfxEvadc_GatingMode_always;
    // We trigger on Rising Edge of the GTM Pulse (which happens at PWM Center)
    groupConfig.queueRequest[0].triggerConfig.triggerMode = IfxEvadc_TriggerMode_uponRisingEdge;
    // Select Source 11 (Connected to GTM ADC Trig 3)
    groupConfig.queueRequest[0].triggerConfig.triggerSource = ADC_TRIGGER_SOURCE;

    IfxEvadc_Adc_initGroup(&g_adc.groupMaster, &groupConfig);


    /* ---------------- SLAVE GROUP (Phase V) ---------------- */
    IfxEvadc_Adc_initGroupConfig(&groupConfig, &g_adc.evadc); // Reset config

    groupConfig.groupId = ADC_GROUP_SLAVE;
    groupConfig.master  = ADC_GROUP_MASTER; // Important: Point to Master
    groupConfig.arbiter.requestSlotQueue0Enabled = TRUE; // Enable Queue

    // Slave does NOT need an External Trigger. It is triggered by the Master Sync signal.
    groupConfig.queueRequest[0].triggerConfig.triggerMode = IfxEvadc_TriggerMode_noExternalTrigger;

    IfxEvadc_Adc_initGroup(&g_adc.groupSlave, &groupConfig);


    /* 3. Channel Config */
    IfxEvadc_Adc_ChannelConfig chConfig;

    /* ---------------- MASTER CHANNEL (Phase U) ---------------- */
    IfxEvadc_Adc_initChannelConfig(&chConfig, &g_adc.groupMaster);

    chConfig.channelId      = ADC_CHN_PHASE_U;
    chConfig.resultRegister = IfxEvadc_ChannelResult_0;
    chConfig.resultPriority = ISR_PRIORITY_ADC; // ISR triggered when Master finishes
    chConfig.resultSrcNr    = IfxEvadc_SrcNr_group0;
    chConfig.resultServProvider = ISR_PROVIDER_ADC;

    // CRITICAL: This enables the Master to wake up the Slaves
    chConfig.synchonize     = TRUE;

    IfxEvadc_Adc_initChannel(&g_adc.chnPhaseU, &chConfig);

    /* ---------------- SLAVE CHANNEL (Phase V) ---------------- */
    IfxEvadc_Adc_initChannelConfig(&chConfig, &g_adc.groupSlave);

    chConfig.channelId      = ADC_CHN_PHASE_V;
    chConfig.resultRegister = IfxEvadc_ChannelResult_0;
    chConfig.resultPriority = 0; // Usually Slave doesn't interrupt, we read all in Master ISR

    // Slave is passive, sync = false
    chConfig.synchonize     = FALSE;

    IfxEvadc_Adc_initChannel(&g_adc.chnPhaseV, &chConfig);


    /* 4. Start Queues */

    // Add Master to Queue (Wait for Trigger)
    IfxEvadc_Adc_addToQueue(&g_adc.chnPhaseU, IfxEvadc_RequestSource_queue0, IFXEVADC_QUEUE_REFILL);

    // Add Slave to Queue (Wait for Sync)
    IfxEvadc_Adc_addToQueue(&g_adc.chnPhaseV, IfxEvadc_RequestSource_queue0, IFXEVADC_QUEUE_REFILL);

    /* 5. Start Analog Converters */
    IfxEvadc_Adc_setAnalogConvertControl(&MODULE_EVADC, &g_adc.groupMaster, IfxEvadc_AnalogConverterMode_normalOperation);
    IfxEvadc_Adc_setAnalogConvertControl(&MODULE_EVADC, &g_adc.groupSlave, IfxEvadc_AnalogConverterMode_normalOperation);
}
