#include <app_config.h>
#include <string.h>
#include <limits.h>
#include <IfxStm.h>

AppConfig app_config;
AppConfig app_config_shadow;
AppSetpoints app_setpoints;
AppState  app_state;

static volatile uint8 config_activation_pending;

#define ADC_STEPS 4096U

void initConfig(void)
{
    memset(&app_config, 0, sizeof(app_config));
    memset(&app_config_shadow, 0, sizeof(app_config_shadow));
    memset(&app_setpoints, 0, sizeof(app_setpoints));
    memset(&app_state,  0, sizeof(app_state));

    /* Safe/default initialization.
     * Tune these values for your hardware/application.
     */

    /* ADC */
    app_config.adc.steps  = ADC_STEPS;
    app_config.adc.supply = 3.3f;

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
    app_config.current.current_sense_factor = 0.01f;

    /* FOC defaults */
    app_config.foc.motor_polepairs   = 7U;
    app_config.foc.calibration_ticks = (uint64)IfxStm_getFrequency(&MODULE_STM0) * 5ULL; //5 seconds
    app_config.foc.resolver_offset   = 0.0f;

    //PI Controller ID
    app_config.foc.pi_config_id.Kp    = 0.05f;
    app_config.foc.pi_config_id.Ki    = 0.05f;
    app_config.foc.pi_config_id.outMax = 0.5f;
    app_config.foc.pi_config_id.outMin = -0.5f;

    //PI Controller IQ
    app_config.foc.pi_config_iq.Kp    = 0.05f;
    app_config.foc.pi_config_iq.Ki    = 0.05f;
    app_config.foc.pi_config_iq.outMax = 0.5f;
    app_config.foc.pi_config_iq.outMin = -0.5f;

    /* Speed PI (output is Iq reference in A) */
    app_config.foc.pi_config_speed.Kp = 0.02f;
    app_config.foc.pi_config_speed.Ki = 0.002f;
    app_config.foc.pi_config_speed.outMax = 8.0f;
    app_config.foc.pi_config_speed.outMin = -8.0f;

    /* State defaults */
    app_state.foc.electricalAngle = 0.0f;
    app_state.foc.calibrated      = false;
    app_state.foc.pi_state_id.integral = 0.0f;
    app_state.foc.pi_state_iq.integral = 0.0f;
    app_state.foc.pi_state_speed.integral = 0.0f;
    app_state.foc.speed_mech_rpm = 0.0f;
    app_state.foc.speed_filt_rpm = 0.0f;
    app_state.foc.speed_iq_ref = 0.0f;

    app_state.resolver.prev_elec_angle = 0.0f;
    app_state.resolver.sector          = 0;

    app_setpoints.foc.speedSetpointRpm = 1200.0f;
    app_setpoints.foc.voltageRef  = 0.15f;
    app_setpoints.foc.id_ref      = 0.0f;
    app_setpoints.foc.iq_ref      = 5.0f;
    app_setpoints.foc.controlMode = CONTROL_MODE_CURRENT;

    app_config_shadow = app_config;
    config_activation_pending = 0U;
}

void commitConfigShadow(void)
{
    requestConfigActivation();
}

void applyConfigShadow(void)
{
    app_config = app_config_shadow;
}

void requestConfigActivation(void)
{
    config_activation_pending = 1U;
}

bool consumeConfigActivationRequest(void)
{
    if (config_activation_pending == 0U)
    {
        return false;
    }

    config_activation_pending = 0U;
    return true;
}

bool isConfigActivationPending(void)
{
    return (config_activation_pending != 0U);
}

void updateFocResolverOffset(float32 resolver_offset)
{
    app_config.foc.resolver_offset = resolver_offset;
    app_config_shadow.foc.resolver_offset = resolver_offset;
}
