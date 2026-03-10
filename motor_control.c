#include "adc.h"
#include "resolver_math.h"
#include <stdio.h>

Resolver_Config_t resolver_cfg;

IFX_INTERRUPT(ISR_Adc_EndOfConversion, 0, ISR_PRIORITY_ADC);
void ISR_Adc_EndOfConversion(void)
{
    uint16 sin_raw, cos_raw, i_u, i_v;

    // 1. Fetch raw data immediately (Fast, no hardware structs exposed)
    Resolver_ADC_Read(&sin_raw, &cos_raw);
    Inverter_ADC_Read(&i_u, &i_v);

    float32 theta_elec = Resolver_Math_GetAngle(&resolver_cfg, sin_raw, cos_raw);

    printf("%f %d %d\n", theta_elec, i_u, i_v);
}
