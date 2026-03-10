#include <currents_adc.h>

#define GROUPID_U IfxEvadc_GroupId_2
#define GROUPID_V IfxEvadc_GroupId_3
#define CHID_U    0
#define CHID_V    0

IfxEvadc_Adc_Group   adc_group_u;
IfxEvadc_Adc_Group   adc_group_v;
IfxEvadc_Adc_Channel adc_channel_u;
IfxEvadc_Adc_Channel adc_channel_v;

void Inverter_ADC_InitGroups(void)
{
    IfxEvadc_Adc_GroupConfig adcGroupConfig;

    /* Group 2 (SLAVE to Group 0) */
    IfxEvadc_Adc_initGroupConfig(&adcGroupConfig, &g_evadc);
    adcGroupConfig.groupId = GROUPID_U;
    adcGroupConfig.master  = IfxEvadc_GroupId_0;  /* KEY: Slaved to Resolver G0 */
    IfxEvadc_Adc_initGroup(&adc_group_u, &adcGroupConfig);

    /* Group 3 (SLAVE to Group 0) */
    IfxEvadc_Adc_initGroupConfig(&adcGroupConfig, &g_evadc);
    adcGroupConfig.groupId = GROUPID_V;
    adcGroupConfig.master  = IfxEvadc_GroupId_0;  /* KEY: Slaved to Resolver G0 */
    IfxEvadc_Adc_initGroup(&adc_group_v, &adcGroupConfig);
}

void Inverter_ADC_InitChannels(void)
{
    IfxEvadc_Adc_ChannelConfig adcChannelConfig;

    /* Slave Channel U */
    IfxEvadc_Adc_initChannelConfig(&adcChannelConfig, &adc_group_u);
    adcChannelConfig.channelId = (IfxEvadc_ChannelId)(CHID_U);
    adcChannelConfig.resultRegister = (IfxEvadc_ChannelResult)(CHID_U);
    IfxEvadc_Adc_initChannel(&adc_channel_u, &adcChannelConfig);

    /* Slave Channel V */
    IfxEvadc_Adc_initChannelConfig(&adcChannelConfig, &adc_group_v);
    adcChannelConfig.channelId = (IfxEvadc_ChannelId)(CHID_V);
    adcChannelConfig.resultRegister = (IfxEvadc_ChannelResult)(CHID_V);
    IfxEvadc_Adc_initChannel(&adc_channel_v, &adcChannelConfig);
}

void Inverter_ADC_Read(uint16* i_u, uint16* i_v)
{
    // The ISR guarantees the conversion is done, so we just read the result directly.
    *i_u = IfxEvadc_Adc_getResult(&adc_channel_u).B.RESULT;
    *i_v = IfxEvadc_Adc_getResult(&adc_channel_v).B.RESULT;
}
