#include "adc.h"
#include "resolver_math.h"
#include <stdio.h>
#include "foc.h"

Resolver_Config_t resolver_cfg;

IFX_INTERRUPT(ISR_Adc_EndOfConversion, 0, ISR_PRIORITY_ADC);
void ISR_Adc_EndOfConversion(void)
{
    uint16 sin_raw, cos_raw, curr_u_raw, curr_u_raw;

    /* 1. Read ADC immediately */
    Resolver_ADC_Read(&sin_raw, &cos_raw);
    Inverter_ADC_Read(&curr_u_raw, &curr_v_raw);

    /* 2. Resolver angle */
    float32 theta_elec = Resolver_Math_GetAngle(&resolver_cfg, sin_raw, cos_raw);

    /* 3. Currents */
    float32 curr_u = curr_u_raw * 5.0f/4096; //some more math needed here
    float32 curr_v = curr_v_raw * 5.0f/4096;

    /* 3. Run FOC */
    focCurrentControlClosedLoop(theta_elec, curr_u, curr_v);
}

