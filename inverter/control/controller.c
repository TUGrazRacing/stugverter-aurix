#include <controller.h>
#include <init/pwm.h>
#include "adc.h"
#include "resolver_math.h"
#include "foc.h"
#include "current_math.h"
#include "gate_driver.h"
#include "stream.h"
#include "speed.h"
#include "IfxCpu.h"
#include "IfxStm.h"

IFX_INTERRUPT(controllerStep, 0, ISR_PRIORITY_ADC);

typedef struct
{
    uint16 curr_u_raw;
    uint16 curr_v_raw;
    uint16 sin_raw;
    uint16 cos_raw;
} ControllerAdcSample;

typedef struct
{
    ControllerAdcSample adc_sample;
    float32 theta_resolver_mech;
} ControllerRuntime;

static volatile uint32 g_control_loop_counter;
static volatile uint64 g_control_loop_last_tick;
static ControllerRuntime controller_runtime;

static void controllerApplyPendingConfig(void);
static void controllerReadAdcSample(void);
static void controllerUpdateSpeedMeasurement(void);
static void controllerRunCurrentLoop(void);
static void controllerPublishLoopTiming(void);
static uint16 controllerCurrentToAdcSteps(float32 current, uint16 offset_adcsteps);

void controllerStep(void)
{
    controllerReadAdcSample();
    controllerUpdateSpeedMeasurement();
    controllerRunCurrentLoop();
    controllerPublishLoopTiming();
    Stream_OnControlLoop();
}

void controllerInit(void)
{
    initConfig(); //get default config
    Stream_Init();
    Speed_Reset(&app_state.foc);

    gatedriverInit();
    initCurrents(&app_config.current, &app_config.adc);
    resolverInit(&app_config.resolver, &app_state.resolver);
    focInit(&app_config.foc, &app_state.foc);
    adcInit();
    pwmInit(&app_config.pwm);

    gatedriverEnable();

    controller_runtime.theta_resolver_mech = 0.0f;
    g_control_loop_counter = 0U;
    g_control_loop_last_tick = (uint64)IfxStm_get(&MODULE_STM0);
    app_state.foc.control_loop_counter = g_control_loop_counter;
    app_state.foc.control_loop_tick = g_control_loop_last_tick;
}

void controllerBackgroundTask(void)
{
    if (!consumeConfigActivationRequest())
    {
        return;
    }

    controllerApplyPendingConfig();
}

void controllerSpeedControlTask(void)
{
    Speed_RunControlTask(&app_config.foc,
                         &app_setpoints.foc,
                         &app_state.foc,
                         controllerGetLoopCounter(),
                         (float32)app_config.pwm.frequency,
                         app_state.foc.resolver_mech_angle);
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
    Speed_Reset(&app_state.foc);

    gatedriverEnable();

    IfxCpu_restoreInterrupts(interrupt_state);
}

static void controllerReadAdcSample(void)
{
    readAdc(&controller_runtime.adc_sample.curr_u_raw,
            &controller_runtime.adc_sample.curr_v_raw,
            &controller_runtime.adc_sample.sin_raw,
            &controller_runtime.adc_sample.cos_raw);

    app_state.foc.adc_curr_u_raw = controller_runtime.adc_sample.curr_u_raw;
    app_state.foc.adc_curr_v_raw = controller_runtime.adc_sample.curr_v_raw;
    app_state.foc.adc_sin_raw = controller_runtime.adc_sample.sin_raw;
    app_state.foc.adc_cos_raw = controller_runtime.adc_sample.cos_raw;
    app_state.foc.adc_curr_u = controller_runtime.adc_sample.curr_u_raw;
    app_state.foc.adc_curr_v = controller_runtime.adc_sample.curr_v_raw;
}

static void controllerUpdateSpeedMeasurement(void)
{
    controller_runtime.theta_resolver_mech = resolverGetMechanicalAngle(controller_runtime.adc_sample.sin_raw,
                                                                        controller_runtime.adc_sample.cos_raw);
    app_state.foc.resolver_mech_angle = controller_runtime.theta_resolver_mech;
}

static void controllerRunCurrentLoop(void)
{
    ThreePhaseCurrents currents_raw;
    ThreePhaseDuty dutycycles;

    currentsGet(&currents_raw, controller_runtime.adc_sample.curr_u_raw, controller_runtime.adc_sample.curr_v_raw);

    /* W current is reconstructed from the two measured phase currents. */
    app_state.foc.adc_curr_w = controllerCurrentToAdcSteps(currents_raw.w,
                                                           (uint16)(((uint32)app_config.current.offset_u_adcsteps
                                                                   + (uint32)app_config.current.offset_v_adcsteps) / 2U));

    app_state.foc.i_uvw = currents_raw;
    focStep(&dutycycles, controller_runtime.theta_resolver_mech, &currents_raw);
    setDutyCycles(dutycycles.u * 100.0f, dutycycles.v * 100.0f, dutycycles.w * 100.0f);
}

static void controllerPublishLoopTiming(void)
{
    g_control_loop_last_tick = (uint64)IfxStm_get(&MODULE_STM0);
    g_control_loop_counter++;
    app_state.foc.control_loop_tick = g_control_loop_last_tick;
    app_state.foc.control_loop_counter = g_control_loop_counter;
}

static uint16 controllerCurrentToAdcSteps(float32 current, uint16 offset_adcsteps)
{
    float32 factor;
    float32 adc_steps;

    if ((app_config.adc.steps == 0U) || (app_config.adc.supply <= 0.0f) || (app_config.current.current_sense_factor <= 0.0f))
    {
        return offset_adcsteps;
    }

    factor = app_config.adc.supply / (((float32)app_config.adc.steps) * app_config.current.current_sense_factor);
    adc_steps = ((current / factor) + (float32)offset_adcsteps) + 0.5f;

    if (adc_steps <= 0.0f)
    {
        return 0U;
    }

    if (adc_steps >= (float32)(app_config.adc.steps - 1U))
    {
        return (uint16)(app_config.adc.steps - 1U);
    }

    return (uint16)adc_steps;
}
