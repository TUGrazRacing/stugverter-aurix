#ifndef ADC_H
#define ADC_H

#include <types_config.h>
#include "Ifx_Types.h"
#include "IfxEvadc_Adc.h"

#define ISR_PRIORITY_ADC           10
#define ISR_PROVIDER_ADC           IfxSrc_Tos_cpu0

void adcInit(void);

void readAdc(uint16 *g0, uint16 *g1, uint16 *g2, uint16 *g3);

#endif /* ADC_H */
