#include "foc.h"
#include "IfxPort.h"
#include "IfxStm.h"
#include "foc_math.h"
#include "pi.h"
#include <math.h>
#include "pwm.h"
#include "logger.h"
#include <stdbool.h>

static FocConfig *foc_config = NULL_PTR;
static FocState  *foc_state  = NULL_PTR;

static void focOpenLoop(ThreePhaseDuty *dutycycles);
static void focCurrentControlClosedLoop(ThreePhaseDuty *dutycycles, float32 theta, const ThreePhaseCurrents *currents);
static void focCalibrateZeroOffset(float32 theta_measured);

void focInit(FocConfig *config, FocState *state)
{
    foc_config = config;
    foc_state  = state;
}

void focStep(ThreePhaseDuty *dutycycles, float32 theta_resolver_mech, const ThreePhaseCurrents *currents)
{
    if ((foc_config == NULL_PTR) || (foc_state == NULL_PTR) || (dutycycles == NULL_PTR))
    {
        return;
    }

    if (foc_state->calibrated)
    {
        focCurrentControlClosedLoop(dutycycles, theta_resolver_mech, currents);
    }
    else
    {
        focCalibrateZeroOffset(theta_resolver_mech);
        *dutycycles = foc_state->duty_3ph;
    }
}

static void focOpenLoop(ThreePhaseDuty *dutycycles)
{
    float32 sinVal, cosVal;

    if ((foc_config == NULL_PTR) || (foc_state == NULL_PTR) || (dutycycles == NULL_PTR) || (pwm_config == NULL_PTR))
    {
        return;
    }

    foc_state->electricalAngle += (foc_config->speedRefHz * FOC_TWO_PI * (1.0f / (float32)pwm_config->frequency));

    if (foc_state->electricalAngle >= FOC_TWO_PI)
    {
        foc_state->electricalAngle -= FOC_TWO_PI;
    }
    else if (foc_state->electricalAngle < 0.0f)
    {
        foc_state->electricalAngle += FOC_TWO_PI;
    }

    sinVal = sinf(foc_state->electricalAngle);
    cosVal = cosf(foc_state->electricalAngle);

    foc_state->v_ab.alpha = foc_config->voltageRef * cosVal;
    foc_state->v_ab.beta  = foc_config->voltageRef * sinVal;

    focSVPWM(&foc_state->v_ab, &foc_state->duty_3ph);

    *dutycycles = foc_state->duty_3ph;
}

static void focCurrentControlClosedLoop(ThreePhaseDuty *dutycycles, float32 theta, const ThreePhaseCurrents *currents)
{
    float32 sinVal, cosVal;
    float32 theta_corr;

    if ((foc_config == NULL_PTR) || (foc_state == NULL_PTR) || (dutycycles == NULL_PTR) || (currents == NULL_PTR))
    {
        return;
    }

    theta_corr = focGetMotorElecAngle(theta, foc_config->resolver_offset, foc_config->motor_polepairs);

    FOC_ClarkeTransform(currents, &foc_state->i_ab);

    sinVal = sinf(theta_corr);
    cosVal = cosf(theta_corr);
    FOC_ParkTransform(&foc_state->i_ab, &foc_state->i_dq, sinVal, cosVal);

    foc_state->v_dq.d = PI_Run(&foc_config->pi_config_id, &foc_state->pi_state_id, foc_config->id_ref, foc_state->i_dq.d);
    foc_state->v_dq.q = PI_Run(&foc_config->pi_config_iq, &foc_state->pi_state_iq, foc_config->iq_ref, foc_state->i_dq.q);

    FOC_InvParkTransform(&foc_state->v_dq, &foc_state->v_ab, sinVal, cosVal);
    focSVPWM(&foc_state->v_ab, &foc_state->duty_3ph);

    *dutycycles = foc_state->duty_3ph;
}

static void focCalibrateZeroOffset(float32 theta_measured)
{
    const float32 alignTheta   = 0.0f;
    const float32 alignVoltage = 0.15f;

    float32 sinVal, cosVal;
    static uint64 starttime = 0U;
    uint64 current_time = IfxStm_get(&MODULE_STM0);

    if ((foc_config == NULL_PTR) || (foc_state == NULL_PTR))
    {
        return;
    }

    if (starttime == 0U)
    {
        starttime = current_time;
    }

    foc_state->v_dq.d = alignVoltage;
    foc_state->v_dq.q = 0.0f;

    sinVal = sinf(alignTheta);
    cosVal = cosf(alignTheta);

    FOC_InvParkTransform(&foc_state->v_dq, &foc_state->v_ab, sinVal, cosVal);
    focSVPWM(&foc_state->v_ab, &foc_state->duty_3ph);

    setDutyCycles(
        foc_state->duty_3ph.u * 100.0f,
        foc_state->duty_3ph.v * 100.0f,
        foc_state->duty_3ph.w * 100.0f
    );

    if ((current_time - starttime) > foc_config->calibration_ticks)
    {
        foc_config->resolver_offset = theta_measured;
        foc_state->calibrated = true;
        starttime = 0U;
    }
}
