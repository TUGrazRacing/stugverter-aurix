#ifndef RESOLVER_ADC_H
#define RESOLVER_ADC_H

#include "adc.h"

void Resolver_ADC_InitGroups(void);
void Resolver_ADC_InitChannels(void);
void Resolver_ADC_SetupQueue(void);
void Resolver_ADC_StartQueue(void);

#endif
