#ifndef INVERTER_ADC_H
#define INVERTER_ADC_H

#include "adc.h"

void Inverter_ADC_InitGroups(void);
void Inverter_ADC_InitChannels(void);
void Inverter_ADC_Read(Ifx_EVADC_G_RES* resultU, Ifx_EVADC_G_RES* resultV);

#endif
