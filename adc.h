/**********************************************************************************************************************
 * \file adc.h
 * \brief EVADC configuration for Synchronized 3-Phase Current Sensing
 *********************************************************************************************************************/
#ifndef ADC_H_
#define ADC_H_

#include "Ifx_Types.h"
#include "IfxEvadc.h"
#include "IfxEvadc_Adc.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/

/* --- Hardware Abstraction Settings --- */

/* Group Selection: Master triggers Slave */
#define ADC_GROUP_MASTER        IfxEvadc_GroupId_0  /* Phase U Group */
#define ADC_GROUP_SLAVE         IfxEvadc_GroupId_1  /* Phase V Group */

/* Channel Selection */
#define ADC_CHN_PHASE_U         IfxEvadc_ChannelId_0 /* Physical Pin AN0 */
#define ADC_CHN_PHASE_V         IfxEvadc_ChannelId_1 /* Physical Pin AN9 (typically on Grp1) */

/* Interrupt Settings */
#define ISR_PRIORITY_ADC        10
#define ISR_PROVIDER_ADC        IfxSrc_Tos_cpu0

/* Trigger Source Mapping
 * IMPORTANT: This must match the hardware connection "GTM_ADC_TRTx" for ATOM1 CH0.
 * On many TC3xx kits, GTM Output 0 maps to Trigger Source 0 or 9.
 * Let's assume Trigger Source 0 (GTM_ADC_TRG0) for this example.
 */
#define ADC_TRIGGER_SOURCE      IfxEvadc_TriggerSource_0

/*********************************************************************************************************************/
/*--------------------------------------------------Data Structures--------------------------------------------------*/
/*********************************************************************************************************************/
typedef struct
{
    IfxEvadc_Adc            evadc;          /* EVADC Module Handle */
    IfxEvadc_Adc_Group      groupMaster;    /* Master Group Handle */
    IfxEvadc_Adc_Group      groupSlave;     /* Slave Group Handle */
    IfxEvadc_Adc_Channel    chnPhaseU;      /* Phase U Channel Handle */
    IfxEvadc_Adc_Channel    chnPhaseV;      /* Phase V Channel Handle */
} App_Adc_Type;

/*********************************************************************************************************************/
/*-----------------------------------------------Function Prototypes-------------------------------------------------*/
/*********************************************************************************************************************/

/**
 * \brief Initializes the EVADC for parallel sampling triggered by GTM.
 */
void initEvadc(void);

/**
 * \brief ISR for ADC End of Conversion.
 */
IFX_EXTERN IFX_INTERRUPT(ISR_Adc_EndOfConversion, 0, ISR_PRIORITY_ADC);

#endif /* ADC_H_ */
