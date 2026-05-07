#include "adc.h"

#define MASTER_GROUP                 IfxEvadc_GroupId_0

#define ADC_QUEUE                    IfxEvadc_RequestSource_queue0
#define ADC_INPUT_CLASS              IfxEvadc_InputClasses_group0
#define ADC_RESULT_REGISTER          IfxEvadc_ChannelResult_0
#define ADC_TRIGGER_SOURCE           IfxEvadc_TriggerSource_11
#define ADC_TRIGGER_MODE             IfxEvadc_TriggerMode_uponRisingEdge
#define ADC_GATING_MODE              IfxEvadc_GatingMode_always
#define ADC_SAMPLE_TIME_SECONDS      ((float32)IFXEVADC_SAMPLETIME_MIN / (float32)IFXEVADC_ANALOG_FREQUENCY_MAX)
#define ADC_QUEUE_EXTERNAL_TRIGGER   (1u << IFX_EVADC_G_Q_QINR_EXTR_OFF)

/* The current iLLD does not expose a wrapper for GxALIAS. Keep the pin mapping
 * in one place until the library grows a channel alias API.
 */
#define G0_ALIAS_CH                  IfxEvadc_ChannelId_2
#define G1_ALIAS_CH                  IfxEvadc_ChannelId_0
#define G2_ALIAS_CH                  IfxEvadc_ChannelId_0
#define G3_ALIAS_CH                  IfxEvadc_ChannelId_0

typedef struct
{
    IfxEvadc_Adc_Group group;
    IfxEvadc_Adc_Channel channel;
} AdcGroupRuntime;

static IfxEvadc_Adc adc;
static AdcGroupRuntime adc_groups[4];

static void adcInitModule(void)
{
    IfxEvadc_Adc_Config config;

    IfxEvadc_Adc_initModuleConfig(&config, &MODULE_EVADC);
    config.analogClockGenerationMode = IfxEvadc_AnalogClockGenerationMode_unsynchronized;
    config.supplyVoltage = IfxEvadc_SupplyVoltageLevelControl_automaticControl;
    config.startupCalibrationControl = IfxEvadc_StartupCalibration_noAction;
    config.globalInputClass[0].conversionMode = IfxEvadc_ChannelNoiseReduction_standardConversion;
    config.globalInputClass[1].conversionMode = IfxEvadc_ChannelNoiseReduction_standardConversion;

    (void)IfxEvadc_Adc_initModule(&adc, &config);
}

static void adcConfigureFastGroupClass(IfxEvadc_Adc_GroupConfig *config)
{
    uint8 input_class;

    config->analogFrequency = IFXEVADC_ANALOG_FREQUENCY_MAX;
    config->disablePostCalibration = TRUE;
    config->doubleClockForMSBConversionSelection = FALSE;
    config->sampleSynchronizationEnabled = IfxEvadc_SampleSynchronization_off;
    config->analogClockSynchronizationDelay = IfxEvadc_AnalogClockSynchronizationDelay_0;
    config->calibrationSampleTimeControlMode = IfxEvadc_CalibrationSampleTimeControl_2;
    config->referencePrechargeEnabled = FALSE;
    config->inputBufferEnabled = FALSE;
    config->analogConverterMode = IfxEvadc_AnalogConverterMode_normalOperation;
    config->inputClasses = ADC_INPUT_CLASS;

    for (input_class = 0u; input_class < IFXEVADC_NUM_INPUTCLASSES; input_class++)
    {
        config->inputClass[input_class].sampleTime = ADC_SAMPLE_TIME_SECONDS;
        config->inputClass[input_class].conversionMode = IfxEvadc_ChannelNoiseReduction_standardConversion;
        config->inputClass[input_class].analogInputPrechargeControlStandard =
            IfxEvadc_AnalogInputPrechargeControl_noPrecharge;
    }
}

static void adcInitGroup(IfxEvadc_GroupId group_id, boolean master)
{
    IfxEvadc_Adc_GroupConfig config;
    AdcGroupRuntime *runtime = &adc_groups[(uint8)group_id];

    IfxEvadc_Adc_initGroupConfig(&config, &adc);
    config.groupId = group_id;
    config.master = master ? group_id : MASTER_GROUP;

    adcConfigureFastGroupClass(&config);

    if (master)
    {
        config.startupCalibration = TRUE;
        config.arbiter.requestSlotQueue0Enabled = TRUE;
        config.queueRequest[0].triggerConfig.gatingMode = ADC_GATING_MODE;
        config.queueRequest[0].triggerConfig.triggerMode = ADC_TRIGGER_MODE;
        config.queueRequest[0].triggerConfig.triggerSource = ADC_TRIGGER_SOURCE;
        config.queueRequest[0].requestSlotPrio = IfxEvadc_RequestSlotPriority_highest;
        config.queueRequest[0].requestSlotStartMode = IfxEvadc_RequestSlotStartMode_cancelInjectRepeat;
    }

    (void)IfxEvadc_Adc_initGroup(&runtime->group, &config);
}

static void adcInitChannel(IfxEvadc_GroupId group_id, boolean master)
{
    IfxEvadc_Adc_ChannelConfig config;
    AdcGroupRuntime *runtime = &adc_groups[(uint8)group_id];

    IfxEvadc_Adc_initChannelConfig(&config, &runtime->group);
    config.channelId = IfxEvadc_ChannelId_0;
    config.inputClass = ADC_INPUT_CLASS;
    config.resultRegister = ADC_RESULT_REGISTER;
    config.resultPriority = master ? ISR_PRIORITY_ADC : 0;
    config.resultServProvider = ISR_PROVIDER_ADC;
    config.resultSrcNr = IfxEvadc_SrcNr_group0;
    config.synchonize = master;
    config.dataModificationMode = IfxEvadc_DataModificationMode_standardDataReduction;
    config.dataReductionControlMode = IfxEvadc_DataReductionControlMode_0;
    config.waitForReadMode = IfxEvadc_WaitForRead_overwriteMode;
    config.fifoMode = IfxEvadc_FifoMode_seperateResultRegister;

    (void)IfxEvadc_Adc_initChannel(&runtime->channel, &config);
}

static void adcForceNoInputPrecharge(IfxEvadc_GroupId group_id)
{
    uint8 input_class;
    Ifx_EVADC_G *group = adc_groups[(uint8)group_id].group.group;

    IfxEvadc_enableAccess(&MODULE_EVADC, (IfxEvadc_Protection)(IfxEvadc_Protection_initGroup0 + group_id));

    for (input_class = 0u; input_class < IFXEVADC_NUM_INPUTCLASSES; input_class++)
    {
        IfxEvadc_setAnalogInputPrechargeControlStandard(group,
                                                         input_class,
                                                         IfxEvadc_AnalogInputPrechargeControl_noPrecharge);
    }

    IfxEvadc_disableAccess(&MODULE_EVADC, (IfxEvadc_Protection)(IfxEvadc_Protection_initGroup0 + group_id));
}

static void adcApplyChannelAlias(IfxEvadc_GroupId group_id, IfxEvadc_ChannelId channel_id)
{
    IfxEvadc_enableAccess(&MODULE_EVADC, (IfxEvadc_Protection)(IfxEvadc_Protection_initGroup0 + group_id));
    MODULE_EVADC.G[group_id].ALIAS.U = 0u;
    MODULE_EVADC.G[group_id].ALIAS.B.ALIAS0 = channel_id;
    IfxEvadc_disableAccess(&MODULE_EVADC, (IfxEvadc_Protection)(IfxEvadc_Protection_initGroup0 + group_id));
}

static void adcInitAlias(void)
{
    adcApplyChannelAlias(IfxEvadc_GroupId_0, G0_ALIAS_CH);
    adcApplyChannelAlias(IfxEvadc_GroupId_1, G1_ALIAS_CH);
    adcApplyChannelAlias(IfxEvadc_GroupId_2, G2_ALIAS_CH);
    adcApplyChannelAlias(IfxEvadc_GroupId_3, G3_ALIAS_CH);
}

void adcInit(void)
{
    adcInitModule();

    adcInitGroup(IfxEvadc_GroupId_1, FALSE);
    adcInitGroup(IfxEvadc_GroupId_2, FALSE);
    adcInitGroup(IfxEvadc_GroupId_3, FALSE);
    adcInitGroup(IfxEvadc_GroupId_0, TRUE);

    adcInitChannel(IfxEvadc_GroupId_0, TRUE);
    adcInitChannel(IfxEvadc_GroupId_1, FALSE);
    adcInitChannel(IfxEvadc_GroupId_2, FALSE);
    adcInitChannel(IfxEvadc_GroupId_3, FALSE);

    adcForceNoInputPrecharge(IfxEvadc_GroupId_0);
    adcForceNoInputPrecharge(IfxEvadc_GroupId_1);
    adcForceNoInputPrecharge(IfxEvadc_GroupId_2);
    adcForceNoInputPrecharge(IfxEvadc_GroupId_3);

    adcInitAlias();
    IfxEvadc_Adc_addToQueue(&adc_groups[0].channel,
                             ADC_QUEUE,
                             IFXEVADC_QUEUE_REFILL | ADC_QUEUE_EXTERNAL_TRIGGER);
}

void readAdc(uint16 *g0, uint16 *g1, uint16 *g2, uint16 *g3)
{
    *g0 = (uint16)IfxEvadc_Adc_getResult(&adc_groups[0].channel).B.RESULT;
    *g1 = (uint16)IfxEvadc_Adc_getResult(&adc_groups[1].channel).B.RESULT;
    *g2 = (uint16)IfxEvadc_Adc_getResult(&adc_groups[2].channel).B.RESULT;
    *g3 = (uint16)IfxEvadc_Adc_getResult(&adc_groups[3].channel).B.RESULT;
}
