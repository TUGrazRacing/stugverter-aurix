#ifndef ADC_H_
#define ADC_H_

#include "Ifx_Types.h"
#include "IfxEvadc_Adc.h"

/* ... Existing Macros ... */
#define ADC_GROUP_IDX           IfxEvadc_GroupId_0
#define ADC_CH_U                IfxEvadc_ChannelId_0  /* AN0 */
#define ADC_CH_V                IfxEvadc_ChannelId_2  /* AN2 */
#define ADC_CH_W                IfxEvadc_ChannelId_3  /* AN3 */

#define ISR_PRIORITY_ADC        (10)
#define ISR_PROVIDER_ADC        IfxSrc_Tos_cpu0

/* ... Existing AppAdc Struct ... */
typedef struct
{
    IfxEvadc_Adc            evadc;
    IfxEvadc_Adc_Group      adcGroup;
    IfxEvadc_Adc_Channel    adcCh[3];
    float32                 results[3];
} AppAdc;

/* --- NEW SHARED STRUCTURE --- */
typedef struct
{
    float32 u;
    float32 v;
    float32 w;
    boolean newData; /* Flag to signal Core 1 */
} AdcSharedData;

/* Global Variables */
IFX_EXTERN AppAdc g_appAdc;
IFX_EXTERN volatile AdcSharedData g_adcSharedData; /* Volatile is crucial for multi-core */

/* Function Prototypes */
void initEvadc(void);
void startEvadcConversion(void);

#endif /* ADC_H_ */
