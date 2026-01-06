#ifndef ADC_H_
#define ADC_H_

#include "IfxEvadc_Adc.h"
#include "Ifx_Types.h"

void initADC(void);

/* Latest phase currents (raw or scaled) */
extern float32 g_i_u;
extern float32 g_i_v;

#endif