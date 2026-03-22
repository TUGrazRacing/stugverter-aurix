#include "app_config.h"
#include <string.h>
#include <limits.h>
#include <IfxStm.h>

AppConfig app_config;
AppState  app_state;

#define ADC_STEPS 4096U

void initConfig(void)
{
    memset(&app_config, 0, sizeof(app_config));
    memset(&app_state,  0, sizeof(app_state));

    /* Safe/default initialization.
     * Tune these values for your hardware/application.
     */

    /* ADC */
    app_config.adc.steps  = ADC_STEPS;
    app_config.adc.supply = 5.0f;

    /* PWM */
    app_config.pwm.frequency = 20e3;

    /* Resolver calibration defaults */
    app_config.resolver.sin_min = ADC_STEPS-1;
    app_config.resolver.cos_min = ADC_STEPS-1;
    app_config.resolver.sin_max = 0U;
    app_config.resolver.cos_max = 0U;
    app_config.resolver.offset_sin = 2050U;
    app_config.resolver.offset_cos = 2050U;
    app_config.resolver.pole_pairs = 4U;

    /* Current sensor offsets */
    app_config.current.offset_u_adcsteps = 1361U;
    app_config.current.offset_v_adcsteps = 1366U;
    app_config.current.current_sense_factor = 0.01; //V/A

    /* FOC defaults */
    app_config.foc.motor_polepairs   = 7U;
    app_config.foc.speedRefHz        = 15.0f;
    app_config.foc.voltageRef        = 0.125f;
    app_config.foc.calibration_ticks = (uint64)IfxStm_getFrequency(&MODULE_STM0) * 5ULL; //5 seconds
    app_config.foc.resolver_offset   = 0.0f;
    app_config.foc.id_ref            = 0.0f;
    app_config.foc.iq_ref            = 5.0f;

    //PI Controller ID
    app_config.foc.pi_config_id.Kp    = 0.2f;
    app_config.foc.pi_config_id.Ki    = 0.001f;
    app_config.foc.pi_config_id.outMax = 0.5f;
    app_config.foc.pi_config_id.outMin = -0.5f;

    //PI Controller IQ
    app_config.foc.pi_config_iq.Kp    = 0.2f;
    app_config.foc.pi_config_iq.Ki    = 0.001f;
    app_config.foc.pi_config_iq.outMax = 0.5f;
    app_config.foc.pi_config_iq.outMin = -0.5f;

    /* State defaults */
    app_state.foc.electricalAngle = 0.0f;
    app_state.foc.calibrated      = false;
    app_state.foc.pi_state_id.integral = 0.0f;
    app_state.foc.pi_state_iq.integral = 0.0f;

    app_state.resolver.prev_elec_angle = 0.0f;
    app_state.resolver.sector          = 0;
}
