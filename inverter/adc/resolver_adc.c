#include "resolver_adc.h"

#define GROUPID_SIN IfxEvadc_GroupId_0
#define GROUPID_COS IfxEvadc_GroupId_1
#define CHID_SIN    0
#define CHID_COS    0


#define EVADC_REFILL_AND_WAIT_FOR_TRIG  ((1U << IFX_EVADC_G_Q_QINR_RF_OFF) | (1U << IFX_EVADC_G_Q_Q0R_EXTR_OFF))
#define EVADC_TRIGGER_SOURCE            IfxEvadc_TriggerSource_11
#define EVADC_TRIGGER_MODE              IfxEvadc_TriggerMode_uponRisingEdge
#define EVADC_GATING_MODE               IfxEvadc_GatingMode_always

IfxEvadc_Adc_Group   adc_group_sin;
IfxEvadc_Adc_Group   adc_group_cos;
IfxEvadc_Adc_Channel adc_channel_sin;
IfxEvadc_Adc_Channel adc_channel_cos;

void Resolver_ADC_InitGroups(void)
{
    IfxEvadc_Adc_GroupConfig adcGroupConfig;

    /* Group 0 (MASTER OF ALL) */
    IfxEvadc_Adc_initGroupConfig(&adcGroupConfig, &g_evadc);
    adcGroupConfig.groupId = GROUPID_SIN;
    adcGroupConfig.master  = GROUPID_SIN;
    adcGroupConfig.arbiter.requestSlotQueue0Enabled = TRUE;
    adcGroupConfig.queueRequest[0].triggerConfig.gatingMode = IfxEvadc_GatingMode_always;

    /* Trigger from three phase PWM*/
    adcGroupConfig.queueRequest[0].triggerConfig.gatingMode    = EVADC_GATING_MODE;
    adcGroupConfig.queueRequest[0].triggerConfig.triggerMode   = EVADC_TRIGGER_MODE;
    adcGroupConfig.queueRequest[0].triggerConfig.triggerSource = EVADC_TRIGGER_SOURCE;

    IfxEvadc_Adc_initGroup(&adc_group_sin, &adcGroupConfig);

    /* Group 1 (SLAVE to Group 0) */
    IfxEvadc_Adc_initGroupConfig(&adcGroupConfig, &g_evadc);
    adcGroupConfig.groupId = GROUPID_COS;
    adcGroupConfig.master  = GROUPID_SIN; /* Slaved to G0 */
    IfxEvadc_Adc_initGroup(&adc_group_cos, &adcGroupConfig);
}

void Resolver_ADC_InitChannels(void)
{
    IfxEvadc_Adc_ChannelConfig adcChannelConfig;

    /* Master Channel */
    IfxEvadc_Adc_initChannelConfig(&adcChannelConfig, &adc_group_sin);
    adcChannelConfig.channelId = (IfxEvadc_ChannelId)(CHID_SIN);
    adcChannelConfig.resultRegister = (IfxEvadc_ChannelResult)(CHID_SIN);
    adcChannelConfig.synchonize = TRUE; /* Master MUST have this true */

    adcChannelConfig.resultServProvider = ISR_PROVIDER_ADC;
    adcChannelConfig.resultPriority     = ISR_PRIORITY_ADC;
    adcChannelConfig.resultSrcNr        = IfxEvadc_SrcNr_group0;

    IfxEvadc_Adc_initChannel(&adc_channel_sin, &adcChannelConfig);


    /* Slave Channel */
    IfxEvadc_Adc_initChannelConfig(&adcChannelConfig, &adc_group_cos);
    adcChannelConfig.channelId = (IfxEvadc_ChannelId)(CHID_COS);
    adcChannelConfig.resultRegister = (IfxEvadc_ChannelResult)(CHID_COS);
    /* Slave does not need synchronize = TRUE */
    IfxEvadc_Adc_initChannel(&adc_channel_cos, &adcChannelConfig);
}

void Resolver_ADC_SetupQueue(void)
{
    IfxEvadc_Adc_addToQueue(&adc_channel_sin, IfxEvadc_RequestSource_queue0, EVADC_REFILL_AND_WAIT_FOR_TRIG);
}

void Resolver_ADC_StartQueue(void)
{
    IfxEvadc_Adc_startQueue(&adc_group_sin, IfxEvadc_RequestSource_queue0);
}

void Resolver_ADC_Read(uint16* sin, uint16* cos)
{
    *sin = IfxEvadc_Adc_getResult(&adc_channel_sin).B.RESULT;
    *cos = IfxEvadc_Adc_getResult(&adc_channel_cos).B.RESULT;
}
