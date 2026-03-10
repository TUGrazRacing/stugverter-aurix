#ifndef ADC_H
#define ADC_H

#include "Ifx_Types.h"
#include "IfxEvadc_Adc.h"

#define ISR_PRIORITY_ADC           10
#define ISR_PROVIDER_ADC           IfxSrc_Tos_cpu0

/* Global handle so sub-modules can reference the EVADC */
extern IfxEvadc_Adc g_evadc;

void adcInit(void);
void adcSoftwareTriggerConversion(void);

void Inverter_ADC_Read(uint16* i_u, uint16* i_v);
void Resolver_ADC_Read(uint16* sin, uint16* cos);

#endif /* ADC_H */
