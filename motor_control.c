#include "pwm.h"
#include "motor_control.h"
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
                         controllerGetLoopCounter());
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
}

static void controllerUpdateSpeedMeasurement(void)
{
    controller_runtime.theta_resolver_mech = resolverGetMechanicalAngle(controller_runtime.adc_sample.sin_raw,
                                                                        controller_runtime.adc_sample.cos_raw);
    app_state.foc.resolver_mech_angle = controller_runtime.theta_resolver_mech;
    Speed_UpdateMeasurement(&app_state.foc,
                            controller_runtime.theta_resolver_mech,
                            (float32)app_config.pwm.frequency);
}

static void controllerRunCurrentLoop(void)
{
    ThreePhaseCurrents currents;
    ThreePhaseDuty dutycycles;

    currentsGet(&currents, controller_runtime.adc_sample.curr_u_raw, controller_runtime.adc_sample.curr_v_raw);
    app_state.foc.adc_curr_u = currents.u;
    app_state.foc.adc_curr_v = currents.v;
    app_state.foc.adc_curr_w = currents.w;
    app_state.foc.i_uvw = currents;
    focStep(&dutycycles, controller_runtime.theta_resolver_mech, &currents);
    setDutyCycles(dutycycles.u * 100.0f, dutycycles.v * 100.0f, dutycycles.w * 100.0f);
}

static void controllerPublishLoopTiming(void)
{
    g_control_loop_last_tick = (uint64)IfxStm_get(&MODULE_STM0);
    g_control_loop_counter++;
    app_state.foc.control_loop_tick = g_control_loop_last_tick;
    app_state.foc.control_loop_counter = g_control_loop_counter;
}
