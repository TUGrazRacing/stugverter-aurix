#include "speed.h"
#include "pi.h"

#define SPEED_TWO_PI_F 6.283185307179586f
#define SPEED_FILTER_ALPHA 0.15f

typedef struct
{
    boolean measurement_initialized;
    float32 previous_theta_mech_rad;
    uint8 previous_control_mode;
    uint32 last_decimated_counter;
} SpeedRuntime;

static SpeedRuntime speed_runtime;

static float32 Speed_NormalizeDeltaAngle(float32 delta_rad);
static void Speed_RunClosedLoop(const FocConfig *config, FocSetpoints *setpoints, FocState *state);
static void Speed_RunCurrentModeMirror(const FocSetpoints *setpoints, FocState *state);

void Speed_Reset(FocState *state)
{
    speed_runtime.measurement_initialized = FALSE;
    speed_runtime.previous_theta_mech_rad = 0.0f;
    speed_runtime.previous_control_mode = CONTROL_MODE_CURRENT;
    speed_runtime.last_decimated_counter = 0U;

    if (state != NULL_PTR)
    {
        state->speed_mech_rpm = 0.0f;
        state->speed_filt_rpm = 0.0f;
        state->speed_iq_ref = 0.0f;
        state->pi_state_speed.integral = 0.0f;
    }
}

void Speed_UpdateMeasurement(FocState *state, float32 theta_mech_rad, float32 loop_hz)
{
    if (state == NULL_PTR)
    {
        return;
    }

    if ((loop_hz <= 0.0f) || (speed_runtime.measurement_initialized == FALSE))
    {
        state->speed_mech_rpm = 0.0f;
        state->speed_filt_rpm = 0.0f;
        speed_runtime.previous_theta_mech_rad = theta_mech_rad;
        speed_runtime.measurement_initialized = TRUE;
        return;
    }

    {
        float32 delta_theta = Speed_NormalizeDeltaAngle(theta_mech_rad - speed_runtime.previous_theta_mech_rad);
        float32 speed_rpm = (delta_theta * loop_hz * 60.0f) / SPEED_TWO_PI_F;

        state->speed_mech_rpm = speed_rpm;
        state->speed_filt_rpm = (SPEED_FILTER_ALPHA * speed_rpm) + ((1.0f - SPEED_FILTER_ALPHA) * state->speed_filt_rpm);
        speed_runtime.previous_theta_mech_rad = theta_mech_rad;
    }
}

void Speed_RunControlTask(const FocConfig *config,
                          FocSetpoints *setpoints,
                          FocState *state,
                          uint32 control_loop_counter)
{
    uint32 decimated_counter;

    if ((config == NULL_PTR) || (setpoints == NULL_PTR) || (state == NULL_PTR))
    {
        return;
    }

    decimated_counter = control_loop_counter / SPEED_CONTROL_LOOP_DIVIDER;
    if (decimated_counter == speed_runtime.last_decimated_counter)
    {
        return;
    }

    speed_runtime.last_decimated_counter = decimated_counter;

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
    if (speed_runtime.previous_control_mode != CONTROL_MODE_SPEED)
    {
        state->pi_state_speed.integral = 0.0f;
    }

    state->speed_iq_ref = PI_Run(&config->pi_config_speed,
                                 &state->pi_state_speed,
                                 setpoints->speedSetpointRpm,
                                 state->speed_filt_rpm);

    setpoints->iq_ref = state->speed_iq_ref;
}

static void Speed_RunCurrentModeMirror(const FocSetpoints *setpoints, FocState *state)
{
    state->speed_iq_ref = setpoints->iq_ref;
}
