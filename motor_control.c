#include "adc.h"
#include "resolver_math.h"
#include <stdio.h>
#include "foc.h"
#include "logger.h"

Resolver_Config_t resolver_cfg;

IFX_INTERRUPT(ISR_Adc_EndOfConversion, 0, ISR_PRIORITY_ADC);
void ISR_Adc_EndOfConversion(void)
{
    uint16 sin_raw, cos_raw, curr_u_raw, curr_v_raw;

    /* 1. Read ADC immediately */
    Resolver_ADC_Read(&sin_raw, &cos_raw);
    Inverter_ADC_Read(&curr_u_raw, &curr_v_raw);

    /* 2. Resolver angle */
    float32 theta_resolver_mech = resolverGetMechanicalAngle(&resolver_cfg, sin_raw, cos_raw);

    /* 3. Currents */
    float32 curr_u = ((curr_u_raw * 5.0f/4096.0f) - 1.65f) / 0.01f;
    float32 curr_v = ((curr_v_raw * 5.0f/4096.0f) - 1.65f) / 0.01f;


    focRun(theta_resolver_mech, curr_u, curr_v);
}

void controller_init(void)
{
    resolverInit(&resolver_cfg);
    focInit();
}
