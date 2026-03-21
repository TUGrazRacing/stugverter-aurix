#include "adc.h"
#include "resolver_math.h"
#include <stdio.h>
#include "foc.h"
#include "logger.h"
#include "motor_control.h"
#include <stdio.h>

Resolver_Config_t resolver_cfg;

IFX_INTERRUPT(conversionDoneISR, 0, ISR_PRIORITY_ADC);

void conversionDoneISR(void)
{
    uint16 sin_raw, cos_raw, curr_u_raw, curr_v_raw;

    readAdc(&curr_u_raw, &curr_v_raw, &sin_raw, &cos_raw);

    float32 theta_resolver_mech = resolverGetMechanicalAngle(&resolver_cfg, sin_raw, cos_raw);

    logPush(&(LogData_t){sin_raw, cos_raw, theta_resolver_mech});

    /* focRun(theta_resolver_mech, curr_u, curr_v); */
}

void controller_init(void)
{
    resolverInit(&resolver_cfg);
    focInit();
}
