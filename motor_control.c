#include "pwm.h"
#include "motor_control.h"
#include "adc.h"
#include "resolver_math.h"
#include "foc.h"
#include "current_math.h"
#include "pwm.h"
#include "gate_driver.h"
#include "logger.h"

IFX_INTERRUPT(controllerStep, 0, ISR_PRIORITY_ADC);

#define ADC_TO_CURR 0.1221 //(5.0f/4096) / 0.01f

void controllerStep(void)
{
    static uint16 sin_raw, cos_raw, curr_u_raw, curr_v_raw;
    readAdc(&curr_u_raw, &curr_v_raw, &sin_raw, &cos_raw);

    static float32 theta_resolver_mech;
    theta_resolver_mech = resolverGetMechanicalAngle(sin_raw, cos_raw);

    static ThreePhaseCurrents currents;
//    currentsGet(&currents, curr_u_raw, curr_v_raw);
    currents.u = ((float32)curr_u_raw - 1361.0f) * ADC_TO_CURR;
    currents.v = ((float32)curr_v_raw - 1366.0f) * ADC_TO_CURR;
    currents.w = -(currents.u + currents.v);
    static ThreePhaseDuty dutycycles;
    focStep(&dutycycles, theta_resolver_mech, &currents);
    setDutyCycles(dutycycles.u * 100.0f, dutycycles.v * 100.0f, dutycycles.w * 100.0f);
}

void controllerInit(void)
{
    initConfig(); //get default config

    gatedriverInit();
    initCurrents(&app_config.current, &app_config.adc);
    resolverInit(&app_config.resolver, &app_state.resolver);
    focInit(&app_config.foc, &app_state.foc);
    adcInit();
    pwmInit(&app_config.pwm);

    gatedriverReadyMode();
    gatedriverEnable();
}
