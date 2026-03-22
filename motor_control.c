#include "pwm.h"
#include "motor_control.h"
#include "adc.h"
#include "resolver_math.h"
#include "foc.h"
#include "current_math.h"
#include "pwm.h"
#include "gate_driver.h"

IFX_INTERRUPT(controllerStep, 0, ISR_PRIORITY_ADC);

void controllerStep(void)
{
  /*
    static uint16 sin_raw, cos_raw, curr_u_raw, curr_v_raw;
    readAdc(&curr_u_raw, &curr_v_raw, &sin_raw, &cos_raw);

    static float32 theta_resolver_mech;
    theta_resolver_mech = resolverGetMechanicalAngle(sin_raw, cos_raw);
  //float32 speed_resolver_mech = resolverGetSpeedRpm(&app_config.resolver);

    static ThreePhaseCurrents currents;
    currentsGet(&currents, curr_u_raw, curr_v_raw);

    static ThreePhaseDuty dutycycles;
    focStep(&dutycycles, theta_resolver_mech, &currents);
    setDutyCycles(dutycycles.u * 100.0f, dutycycles.v * 100.0f, dutycycles.w * 100.0f);
  */
    printf("I\n");
}

void controllerInit(void)
{
    gatedriverInit();
    resolverInit(&app_config.resolver, &app_state.resolver);
    focInit(&app_config.foc, &app_state.foc);
    adcInit();
    pwmInit(&app_config.pwm);

    gatedriverReadyMode();
    gatedriverEnable();
}
