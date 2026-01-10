#include "adc.h"
/* Note: stdio.h is REMOVED from here. Core 0 no longer prints. */

AppAdc g_appAdc;

/* Define the shared variable */
volatile AdcSharedData g_adcSharedData = {0.0f, 0.0f, 0.0f, FALSE};

IFX_INTERRUPT(ISR_AdcConversionFinished, 0, ISR_PRIORITY_ADC);

void ISR_AdcConversionFinished(void)
{
    Ifx_EVADC_G_RES conversionResult;

    /* 1. Read the Results */
    conversionResult = IfxEvadc_Adc_getResult(&g_appAdc.adcCh[0]);
    g_appAdc.results[0] = (float32)conversionResult.B.RESULT;

    conversionResult = IfxEvadc_Adc_getResult(&g_appAdc.adcCh[1]);
    g_appAdc.results[1] = (float32)conversionResult.B.RESULT;

    conversionResult = IfxEvadc_Adc_getResult(&g_appAdc.adcCh[2]);
    g_appAdc.results[2] = (float32)conversionResult.B.RESULT;

    /* 2. PASS TO CORE 1 (Producer) */
    /* We simply overwrite the shared data with the latest values.
       Core 1 will pick them up when it's ready. */
    g_adcSharedData.u = g_appAdc.results[0];
    g_adcSharedData.v = g_appAdc.results[1];
    g_adcSharedData.w = g_appAdc.results[2];

    /* Set flag to tell Core 1 "Fresh data is here" */
    g_adcSharedData.newData = TRUE;
    IfxPort_setPinLow(&MODULE_P00, 12);
}

/* ... initEvadc and startEvadcConversion remain exactly the same as before ... */
void initEvadc(void)
{
    /* (Copy the initEvadc code from the previous working step here) */
    /* Ensure the Interrupt logic is the one from the previous step (Channel Result Interrupt) */

    IfxEvadc_Adc_Config adcConfig;
    IfxEvadc_Adc_GroupConfig groupConfig;
    IfxEvadc_Adc_ChannelConfig channelConfig[3];

    IfxEvadc_Adc_initModuleConfig(&adcConfig, &MODULE_EVADC);
    IfxEvadc_Adc_initModule(&g_appAdc.evadc, &adcConfig);

    IfxEvadc_Adc_initGroupConfig(&groupConfig, &g_appAdc.evadc);
    groupConfig.groupId = ADC_GROUP_IDX;
    groupConfig.master = ADC_GROUP_IDX;
    groupConfig.arbiter.requestSlotQueue0Enabled = TRUE;
    groupConfig.queueRequest[0].triggerConfig.gatingMode = IfxEvadc_GatingMode_always;

    IfxEvadc_Adc_initGroup(&g_appAdc.adcGroup, &groupConfig);

    IfxEvadc_ChannelId chIds[3] = {ADC_CH_V, ADC_CH_U, ADC_CH_W};

    for(int i=0; i<3; i++)
    {
        IfxEvadc_Adc_initChannelConfig(&channelConfig[i], &g_appAdc.adcGroup);
        channelConfig[i].channelId = chIds[i];
        channelConfig[i].resultRegister = (IfxEvadc_ChannelResult)(i);

        /* Interrupt on Last Channel (AN3) */
        if(i == 2)
        {
            channelConfig[i].resultPriority = ISR_PRIORITY_ADC;
            channelConfig[i].resultServProvider = ISR_PROVIDER_ADC;
            channelConfig[i].resultSrcNr = IfxEvadc_SrcNr_group0;
        }

        IfxEvadc_Adc_initChannel(&g_appAdc.adcCh[i], &channelConfig[i]);
        IfxEvadc_Adc_addToQueue(&g_appAdc.adcCh[i], IfxEvadc_RequestSource_queue0, IFXEVADC_QUEUE_REFILL);
    }
}

void startEvadcConversion(void)
{
    IfxEvadc_Adc_startQueue(&g_appAdc.adcGroup, IfxEvadc_RequestSource_queue0);
}
