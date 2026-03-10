#include "adc.h"
#include "currents_adc.h"
#include "resolver_adc.h"
#include "serialio.h"
#include <stdio.h>

IfxEvadc_Adc g_evadc;

void adcInit(void)
{
    /* 1. Initialize the core EVADC module ONCE */
    IfxEvadc_Adc_Config adcConfig;
    IfxEvadc_Adc_initModuleConfig(&adcConfig, &MODULE_EVADC);
    IfxEvadc_Adc_initModule(&g_evadc, &adcConfig);

    /* 2. Initialize Groups (Resolver Group 0 is the Master to ALL others) */
    Resolver_ADC_InitGroups();
    Inverter_ADC_InitGroups();

    /* 3. Initialize Channels */
    Resolver_ADC_InitChannels();
    Inverter_ADC_InitChannels();

    /* 4. Setup the trigger queue on the single Master */
    Resolver_ADC_SetupQueue();
}

void adcSoftwareTriggerConversion(void)
{
    /* A single trigger fires all 4 synchronized channels */
    Resolver_ADC_StartQueue();
}
