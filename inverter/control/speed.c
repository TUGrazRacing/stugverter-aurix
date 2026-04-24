#include "speed.h"
#include "pi.h"

#define SPEED_TWO_PI_F 6.283185307179586f

typedef struct
{
    boolean measurement_initialized;
    float32 previous_theta_mech_rad;
    uint8 previous_control_mode;
    uint32 last_decimated_counter;
} SpeedRuntime;

static SpeedRuntime speed_runtime;

static float32 Speed_NormalizeDeltaAngle(float32 delta_rad);
static float32 Speed_Clamp(float32 value, float32 min, float32 max);
static float32 Speed_SlewTowards(float32 current, float32 target, float32 max_delta);
static void Speed_RunClosedLoop(const FocConfig *config, FocSetpoints *setpoints, FocState *state, float32 dt_s);
static void Speed_RunCurrentModeMirror(const FocConfig *config, FocSetpoints *setpoints, FocState *state, float32 dt_s);

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
        state->speed_setpoint_ramped_rpm = 0.0f;
        state->iq_ref_ramped = 0.0f;
        state->pi_state_speed.integral = 0.0f;
    }
}

void Speed_UpdateMeasurement(FocState *state, float32 theta_mech_rad, float32 loop_hz, float32 filter_alpha)
{
    float32 alpha;

    if (state == NULL_PTR)
    {
        return;
    }

    alpha = Speed_Clamp(filter_alpha, 0.0f, 1.0f);

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
        state->speed_filt_rpm = (alpha * speed_rpm) + ((1.0f - alpha) * state->speed_filt_rpm);
        speed_runtime.previous_theta_mech_rad = theta_mech_rad;
    }
}

void Speed_RunControlTask(const FocConfig *config,
                          FocSetpoints *setpoints,
                          FocState *state,
                          uint32 control_loop_counter,
                          float32 current_loop_hz)
{
    uint32 decimated_counter;
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
    if (decimated_counter == speed_runtime.last_decimated_counter)
    {
        return;
    }

    speed_runtime.last_decimated_counter = decimated_counter;
    dt_s = ((float32)SPEED_CONTROL_LOOP_DIVIDER) / current_loop_hz;

    if (setpoints->controlMode == CONTROL_MODE_SPEED)
    {
        Speed_RunClosedLoop(config, setpoints, state, dt_s);
    }
    else
    {
        Speed_RunCurrentModeMirror(config, setpoints, state, dt_s);
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

static float32 Speed_Clamp(float32 value, float32 min, float32 max)
{
    if (value < min)
    {
        return min;
    }

    if (value > max)
    {
        return max;
    }

    return value;
}

static float32 Speed_SlewTowards(float32 current, float32 target, float32 max_delta)
{
    float32 delta = target - current;

    if (max_delta <= 0.0f)
    {
        return target;
    }

    if (delta > max_delta)
    {
        delta = max_delta;
    }
    else if (delta < -max_delta)
    {
        delta = -max_delta;
    }

    return current + delta;
}

static void Speed_RunClosedLoop(const FocConfig *config, FocSetpoints *setpoints, FocState *state, float32 dt_s)
{
    float32 iq_ref_raw;
    float32 speed_setpoint_ramped;
    float32 max_speed_step;
    float32 max_iq_step;

    if (speed_runtime.previous_control_mode != CONTROL_MODE_SPEED)
    {
        state->pi_state_speed.integral = 0.0f;
        state->speed_setpoint_ramped_rpm = state->speed_filt_rpm;
        state->iq_ref_ramped = setpoints->iq_ref;
    }

    max_speed_step = config->speed_setpoint_ramp_rpm_per_s * dt_s;
    speed_setpoint_ramped = Speed_SlewTowards(state->speed_setpoint_ramped_rpm,
                                              setpoints->speedSetpointRpm,
                                              max_speed_step);
    state->speed_setpoint_ramped_rpm = speed_setpoint_ramped;

    iq_ref_raw = PI_Run(&config->pi_config_speed,
                        &state->pi_state_speed,
                        speed_setpoint_ramped,
                        state->speed_filt_rpm);

    max_iq_step = config->iq_ref_slew_a_per_s * dt_s;
    state->iq_ref_ramped = Speed_SlewTowards(state->iq_ref_ramped, iq_ref_raw, max_iq_step);
    state->speed_iq_ref = state->iq_ref_ramped;
    setpoints->iq_ref = state->iq_ref_ramped;
}

static void Speed_RunCurrentModeMirror(const FocConfig *config, FocSetpoints *setpoints, FocState *state, float32 dt_s)
{
    float32 max_iq_step;

    if (speed_runtime.previous_control_mode == CONTROL_MODE_SPEED)
    {
        state->iq_ref_ramped = setpoints->iq_ref;
    }

    state->speed_setpoint_ramped_rpm = setpoints->speedSetpointRpm;
    max_iq_step = config->iq_ref_slew_a_per_s * dt_s;
    state->iq_ref_ramped = Speed_SlewTowards(state->iq_ref_ramped, setpoints->iq_ref, max_iq_step);
    state->speed_iq_ref = state->iq_ref_ramped;
    setpoints->iq_ref = state->iq_ref_ramped;
}
