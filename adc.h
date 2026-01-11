/**********************************************************************************************************************
 * \file adc.h
 * \brief EVADC configuration for synchronized sampling:
 *        Group0 dummy master (ISR/trigger) + 2 slave groups measuring phase currents in parallel.
 *********************************************************************************************************************/
#ifndef ADC_H_
#define ADC_H_

#include "Ifx_Types.h"
#include "IfxEvadc.h"
#include "IfxEvadc_Adc.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/

/* ========================= Groups =========================
 * Group0 is a dummy master: it is triggered by GTM and generates the interrupt.
 * The real measurements happen in parallel in the two slave groups.
 */
#define ADC_GROUP_DUMMY_MASTER     IfxEvadc_GroupId_0
#define ADC_GROUP_SLAVE_A          IfxEvadc_GroupId_1
#define ADC_GROUP_SLAVE_B          IfxEvadc_GroupId_2

/* ========================= Channels =========================
 * IMPORTANT:
 * - channelId maps to the EVADC channel inside that group.
 * - which physical AN-pin that corresponds to is MCU/package/board dependent.
 *
 * Update these to your actual phase-current sense inputs.
 */
#define ADC_CHN_DUMMY              IfxEvadc_ChannelId_0   /* Dummy input (not used) */

#define ADC_CHN_PHASE_U            IfxEvadc_ChannelId_0   /* e.g. AN8  (example placeholder) */
#define ADC_CHN_PHASE_V            IfxEvadc_ChannelId_0   /* e.g. AN9  (example placeholder) */

/* ========================= Interrupt Settings ========================= */
#define ISR_PRIORITY_ADC           10
#define ISR_PROVIDER_ADC           IfxSrc_Tos_cpu0

/*********************************************************************************************************************/
/*--------------------------------------------------Data Structures--------------------------------------------------*/
/*********************************************************************************************************************/
typedef struct
{
    IfxEvadc_Adc            evadc;          /* EVADC Module Handle */

    IfxEvadc_Adc_Group      groupDummy;     /* Group0 dummy master */
    IfxEvadc_Adc_Group      groupSlaveA;    /* Slave group A */
    IfxEvadc_Adc_Group      groupSlaveB;    /* Slave group B */

    IfxEvadc_Adc_Channel    chnDummy;       /* Dummy master channel (interrupt source) */
    IfxEvadc_Adc_Channel    chnPhaseA;      /* Real measurement A (slave) */
    IfxEvadc_Adc_Channel    chnPhaseB;      /* Real measurement B (slave) */
} App_Adc_Type;

/*********************************************************************************************************************/
/*-----------------------------------------------Function Prototypes-------------------------------------------------*/
/*********************************************************************************************************************/

/**
 * \brief Initializes the EVADC for synchronized sampling triggered by GTM.
 *        Group0 dummy triggers + ISR; two slaves measure in parallel.
 */
void phaseAdcInit(void);

/**
 * \brief ISR for ADC End of Conversion (from dummy master channel).
 */
IFX_EXTERN IFX_INTERRUPT(ISR_Adc_EndOfConversion, 0, ISR_PRIORITY_ADC);

#endif /* ADC_H_ */
