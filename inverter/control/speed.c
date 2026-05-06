#include "speed.h"
#include "pi.h"

#define SPEED_TWO_PI_F 6.283185307179586f
#define SPEED_MIN_VALID_LOOP_HZ 1.0f

typedef struct
{
    boolean measurement_initialized;
    float32 previous_theta_mech_rad;
    uint8 previous_control_mode;
    uint32 last_decimated_counter;
    boolean control_task_initialized;
} SpeedRuntime;

static SpeedRuntime speed_runtime;

static void Speed_UpdateMeasurement(FocState *state, float32 theta_mech_rad, float32 loop_hz);
static float32 Speed_NormalizeDeltaAngle(float32 delta_rad);
static void Speed_RunClosedLoop(const FocConfig *config, FocSetpoints *setpoints, FocState *state);
static void Speed_RunCurrentModeMirror(FocSetpoints *setpoints, FocState *state);

void Speed_Reset(FocState *state)
{
    speed_runtime.measurement_initialized = FALSE;
    speed_runtime.previous_theta_mech_rad = 0.0f;
    speed_runtime.previous_control_mode = CONTROL_MODE_CURRENT;
    speed_runtime.last_decimated_counter = 0U;
    speed_runtime.control_task_initialized = FALSE;

    if (state != NULL_PTR)
    {
        state->speed_mech_rpm = 0.0f;
        state->speed_iq_ref = 0.0f;
        state->pi_state_speed.integral = 0.0f;
    }
}

void Speed_RunControlTask(const FocConfig *config,
                          FocSetpoints *setpoints,
                          FocState *state,
                          uint32 control_loop_counter,
                          float32 current_loop_hz,
                          float32 theta_mech_rad)
{
    uint32 decimated_counter;
    uint32 decimated_delta;
    float32 dt_s;

    if ((config == NULL_PTR) || (setpoints == NULL_PTR) || (state == NULL_PTR))
    {
        return;
    }

    if (current_loop_hz <= 0.0f)
    {
        return;
    }

    decimated_counter = control_loop_counter / SPEED_CONTROL_LOOP_DIVIDER;
    if (speed_runtime.control_task_initialized == FALSE)
    {
        speed_runtime.last_decimated_counter = decimated_counter;
        speed_runtime.control_task_initialized = TRUE;
        Speed_UpdateMeasurement(state, theta_mech_rad, current_loop_hz / (float32)SPEED_CONTROL_LOOP_DIVIDER);
        return;
    }

    decimated_delta = decimated_counter - speed_runtime.last_decimated_counter;
    if (decimated_delta == 0U)
    {
        return;
    }

    speed_runtime.last_decimated_counter = decimated_counter;
    dt_s = ((float32)SPEED_CONTROL_LOOP_DIVIDER * (float32)decimated_delta) / current_loop_hz;

    Speed_UpdateMeasurement(state, theta_mech_rad, 1.0f / dt_s);

    if (setpoints->controlMode == CONTROL_MODE_SPEED)
    {
        Speed_RunClosedLoop(config, setpoints, state);
    }
    else
    {
        Speed_RunCurrentModeMirror(setpoints, state);
    }

    speed_runtime.previous_control_mode = setpoints->controlMode;
}

static void Speed_UpdateMeasurement(FocState *state, float32 theta_mech_rad, float32 loop_hz)
{
    float32 delta_theta;
    float32 speed_rpm;

    if (state == NULL_PTR)
    {
        return;
    }

    if ((loop_hz <= SPEED_MIN_VALID_LOOP_HZ) || (speed_runtime.measurement_initialized == FALSE))
    {
        state->speed_mech_rpm = 0.0f;
        speed_runtime.previous_theta_mech_rad = theta_mech_rad;
        speed_runtime.measurement_initialized = TRUE;
        return;
    }

    delta_theta = Speed_NormalizeDeltaAngle(theta_mech_rad - speed_runtime.previous_theta_mech_rad);
    speed_rpm = (delta_theta * loop_hz * 60.0f) / SPEED_TWO_PI_F;

    state->speed_mech_rpm = speed_rpm;
    speed_runtime.previous_theta_mech_rad = theta_mech_rad;
}

static float32 Speed_NormalizeDeltaAngle(float32 delta_rad)
{
    while (delta_rad > (SPEED_TWO_PI_F * 0.5f))
    {
        delta_rad -= SPEED_TWO_PI_F;
    }

    while (delta_rad < -(SPEED_TWO_PI_F * 0.5f))
    {
        delta_rad += SPEED_TWO_PI_F;
    }

    return delta_rad;
}

static void Speed_RunClosedLoop(const FocConfig *config, FocSetpoints *setpoints, FocState *state)
{
    float32 iq_ref_raw;

    if (speed_runtime.previous_control_mode != CONTROL_MODE_SPEED)
    {
        state->pi_state_speed.integral = 0.0f;
    }

    iq_ref_raw = PI_Run(&config->pi_config_speed,
                        &state->pi_state_speed,
                        setpoints->speedSetpointRpm,
                        state->speed_mech_rpm);

    state->speed_iq_ref = iq_ref_raw;
    setpoints->iq_ref = iq_ref_raw;
}

static void Speed_RunCurrentModeMirror(FocSetpoints *setpoints, FocState *state)
{
    state->speed_iq_ref = setpoints->iq_ref;
}
