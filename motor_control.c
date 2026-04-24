#include "pwm.h"
#include "motor_control.h"
#include "adc.h"
#include "resolver_math.h"
#include "foc.h"
#include "current_math.h"
#include "pwm.h"
#include "gate_driver.h"
<<<<<<< HEAD
#include "stream.h"
=======
#include "logger.h"
#include "pi.h"
#include "IfxCpu.h"
#include "IfxStm.h"
#include <math.h>
>>>>>>> temp_branch

IFX_INTERRUPT(controllerStep, 0, ISR_PRIORITY_ADC);

#define ADC_TO_CURR 0.1221 //(5.0f/4096) / 0.01f
#define TWO_PI_F 6.283185307179586f
#define SPEED_FILTER_ALPHA 0.15f

static volatile uint32 g_control_loop_counter;
static volatile uint64 g_control_loop_last_tick;

static void controllerApplyPendingConfig(void);

void controllerStep(void)
{
    static uint16 sin_raw, cos_raw, curr_u_raw, curr_v_raw;
    static float32 prev_theta_resolver_mech;
    static bool speed_init_done;

    readAdc(&curr_u_raw, &curr_v_raw, &sin_raw, &cos_raw);
    static float32 theta_resolver_mech;
    theta_resolver_mech = resolverGetMechanicalAngle(sin_raw, cos_raw);

    {
        const float32 loop_hz = (float32)app_config.pwm.frequency;
        if ((loop_hz > 0.0f) && speed_init_done)
        {
            float32 delta_theta = theta_resolver_mech - prev_theta_resolver_mech;
            while (delta_theta > (TWO_PI_F * 0.5f))
            {
                delta_theta -= TWO_PI_F;
            }
            while (delta_theta < -(TWO_PI_F * 0.5f))
            {
                delta_theta += TWO_PI_F;
            }

            {
                float32 speed_rpm = (delta_theta * loop_hz * 60.0f) / TWO_PI_F;
                app_state.foc.speed_mech_rpm = speed_rpm;
                app_state.foc.speed_filt_rpm = (SPEED_FILTER_ALPHA * speed_rpm) + ((1.0f - SPEED_FILTER_ALPHA) * app_state.foc.speed_filt_rpm);
            }
        }
        else
        {
            app_state.foc.speed_mech_rpm = 0.0f;
            app_state.foc.speed_filt_rpm = 0.0f;
            speed_init_done = true;
        }

        prev_theta_resolver_mech = theta_resolver_mech;
    }

    static ThreePhaseCurrents currents;
    currentsGet(&currents, curr_u_raw, curr_v_raw);
    static ThreePhaseDuty dutycycles;
    focStep(&dutycycles, theta_resolver_mech, &currents);
    setDutyCycles(dutycycles.u * 100.0f, dutycycles.v * 100.0f, dutycycles.w * 100.0f);
<<<<<<< HEAD
    Stream_OnControlLoop();
=======

    g_control_loop_last_tick = (uint64)IfxStm_get(&MODULE_STM0);
    g_control_loop_counter++;
>>>>>>> temp_branch
}

void controllerInit(void)
{
    initConfig(); //get default config
    Stream_Init();

    gatedriverInit();
    initCurrents(&app_config.current, &app_config.adc);
    resolverInit(&app_config.resolver, &app_state.resolver);
    focInit(&app_config.foc, &app_state.foc);
    adcInit();
    pwmInit(&app_config.pwm);

    gatedriverEnable();

    g_control_loop_counter = 0U;
    g_control_loop_last_tick = (uint64)IfxStm_get(&MODULE_STM0);
}

void controllerBackgroundTask(void)
{
    if (!consumeConfigActivationRequest())
    {
        return;
    }

    controllerApplyPendingConfig();
}

void controllerCore1Task(void)
{
    static uint8 prev_mode = CONTROL_MODE_CURRENT;
    static uint32 last_decimated_counter;

    const uint32 decimated_counter = controllerGetLoopCounter() / 10U;
    if (decimated_counter == last_decimated_counter)
    {
        return;
    }
    last_decimated_counter = decimated_counter;

    if (app_setpoints.foc.controlMode == CONTROL_MODE_SPEED)
    {
        if (prev_mode != CONTROL_MODE_SPEED)
        {
            app_state.foc.pi_state_speed.integral = 0.0f;
        }

        app_state.foc.speed_iq_ref = PI_Run(&app_config.foc.pi_config_speed,
                                            &app_state.foc.pi_state_speed,
                                            app_setpoints.foc.speedSetpointRpm,
                                            app_state.foc.speed_filt_rpm);

        app_setpoints.foc.iq_ref = app_state.foc.speed_iq_ref;
    }
    else
    {
        app_state.foc.speed_iq_ref = app_setpoints.foc.iq_ref;
    }

    prev_mode = app_setpoints.foc.controlMode;
}

uint32 controllerGetLoopCounter(void)
{
    return g_control_loop_counter;
}

uint64 controllerGetLastLoopTick(void)
{
    return g_control_loop_last_tick;
}

static void controllerApplyPendingConfig(void)
{
    boolean interrupt_state = IfxCpu_disableInterrupts();

    gatedriverDisable();
    applyConfigShadow();

    initCurrents(&app_config.current, &app_config.adc);
    resolverInit(&app_config.resolver, &app_state.resolver);
    focInit(&app_config.foc, &app_state.foc);
    pwmInit(&app_config.pwm);

    app_state.foc.pi_state_id.integral = 0.0f;
    app_state.foc.pi_state_iq.integral = 0.0f;
    app_state.foc.pi_state_speed.integral = 0.0f;

    gatedriverEnable();

    IfxCpu_restoreInterrupts(interrupt_state);
}
