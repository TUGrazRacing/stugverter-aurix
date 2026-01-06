#include "adc.h"

/* ================= ADC HANDLES ================= */

static IfxVadc_Adc vadc;
static IfxVadc_Adc_Group vadcGroup;
static IfxVadc_Adc_Channel vadcIU;
static IfxVadc_Adc_Channel vadcIV;

/* Public current values */
float32 g_i_u = 0.0f;
float32 g_i_v = 0.0f;

/* ================= ADC ISR ================= */

#define ISR_PRIORITY_ADC  30

IFX_INTERRUPT(VADC_G0_ISR, 0, ISR_PRIORITY_ADC)
{
    uint16 rawIU = IfxVadc_Adc_getResult(&vadcIU).B.RESULT;
    uint16 rawIV = IfxVadc_Adc_getResult(&vadcIV).B.RESULT;

    /* Scale to amps (example) */
    g_i_u = (float32)rawIU;
    g_i_v = (float32)rawIV;

    /* ---- FOC CURRENT LOOP GOES HERE ---- */
    /* Clarke → Park → PI → SVPWM → duty update */
}

void initADC(void)
{
    IfxVadc_Adc_Config adcCfg;
    IfxVadc_Adc_initConfig(&adcCfg, &MODULE_VADC);
    IfxVadc_Adc_init(&vadc, &adcCfg);

    /* -------- Group config -------- */
    IfxVadc_Adc_GroupConfig grpCfg;
    IfxVadc_Adc_initGroupConfig(&grpCfg, &vadc);

    grpCfg.groupId = IfxVadc_GroupId_0;
    grpCfg.master = grpCfg.groupId;

    grpCfg.triggerConfig.triggerSource = IfxVadc_TriggerSource_0;  // GTM TRIG0
    grpCfg.triggerConfig.triggerMode   = IfxVadc_TriggerMode_uponRisingEdge;
    grpCfg.triggerConfig.gatingMode    = IfxVadc_GatingMode_always;

    IfxVadc_Adc_initGroup(&vadcGroup, &grpCfg);

    /* -------- Channel IU -------- */
    IfxVadc_Adc_ChannelConfig chCfg;
    IfxVadc_Adc_initChannelConfig(&chCfg, &vadcGroup);

    chCfg.channelId = IfxVadc_ChannelId_0;
    chCfg.resultRegister = 0;

    IfxVadc_Adc_initChannel(&vadcIU, &chCfg);

    /* -------- Channel IV -------- */
    chCfg.channelId = IfxVadc_ChannelId_1;
    chCfg.resultRegister = 1;

    IfxVadc_Adc_initChannel(&vadcIV, &chCfg);

    /* -------- Queue -------- */
    IfxVadc_Adc_addToQueue(&vadcIU, IfxVadc_RequestSource_queue0, IFXADC_QUEUE_REFILL);
    IfxVadc_Adc_addToQueue(&vadcIV, IfxVadc_RequestSource_queue0, IFXADC_QUEUE_REFILL);

    IfxVadc_Adc_enableQueueTrigger(&vadcGroup, IfxVadc_RequestSource_queue0);

    /* -------- Interrupt -------- */
    IfxVadc_Adc_setResultPriority(&vadcGroup, 1, ISR_PRIORITY_ADC);
    IfxVadc_Adc_setResultServiceProvider(&vadcGroup, 1, IfxSrc_Tos_cpu0);
}
