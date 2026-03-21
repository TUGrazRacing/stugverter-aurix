#include "adc.h"
#include "resolver_math.h"
#include <stdio.h>
#include "foc.h"
#include "logger.h"
#include "motor_control.h"
#include <stdio.h>

#define ADC_TO_CURR 0.1221 //(5.0f/4096) / 0.01f

Resolver_Config_t resolver_cfg;

IFX_INTERRUPT(conversionDoneISR, 0, ISR_PRIORITY_ADC);

void conversionDoneISR(void)
{
    uint16 sin_raw, cos_raw, curr_u_raw, curr_v_raw;
    readAdc(&curr_u_raw, &curr_v_raw, &sin_raw, &cos_raw);

    float32 theta_resolver_mech = resolverGetMechanicalAngle(&resolver_cfg, sin_raw, cos_raw);

    float32 i_u = ((float32)curr_u_raw - 1361.0f) * ADC_TO_CURR;
    float32 i_v = ((float32)curr_v_raw - 1366.0f) * ADC_TO_CURR;
//    logPush(&(LogData_t){curr_u_raw, curr_v_raw, theta_resolver_mech});

    focRun(theta_resolver_mech, i_u, i_v);

}

void controller_init(void)
{
    resolverInit(&resolver_cfg);
    focInit();
}
