#include "foc.h"
#include "IfxPort.h"
#include "IfxStm.h"
#include "foc_math.h"
#include "pi.h"
#include <math.h>
#include "pwm.h"
#include "app_config.h"
#include <stdbool.h>
#include <stdio.h>

static FocConfig *foc_config = NULL_PTR;
static FocState *foc_state = NULL_PTR;

/* Notice we removed the dutycycles pointer from the internal functions.
 * They will purely operate on foc_state. */
static void focOpenLoop (void);
static void focCurrentControlClosedLoop (float32 theta, const ThreePhaseCurrents *currents);
static void focCalibrateZeroOffset (float32 theta_measured);
static void focLimitVoltageVector(CurrentsDQ *v_dq, float32 limit);

void focInit (FocConfig *config, FocState *state)
{
  foc_config = config;
  foc_state = state;
}

void focStep (ThreePhaseDuty *dutycycles, float32 theta_resolver_mech, const ThreePhaseCurrents *currents)
{
  if ((foc_config == NULL_PTR) || (foc_state == NULL_PTR) || (dutycycles == NULL_PTR))
  {
    return;
  }

  // 1. Compute the FOC algorithm (updates foc_state->duty_3ph internally)
  if (foc_state->calibrated)
  {
      focCurrentControlClosedLoop(theta_resolver_mech, currents);
      //focOpenLoop();
  }
  else
  {
    focCalibrateZeroOffset(theta_resolver_mech);
  }

  // 2. Output the final computed duty cycle for the controller to use
  // foc_state keeps the values for your logger!
  *dutycycles = foc_state->duty_3ph;
}

static void focOpenLoop (void)
{
  float32 sinVal, cosVal;

  if ((foc_config == NULL_PTR) || (foc_state == NULL_PTR) || (pwm_config == NULL_PTR))
  {
    return;
  }

  foc_state->electricalAngle += ((app_setpoints.foc.speedSetpointRpm / 60.0f) * FOC_TWO_PI * (1.0f / (float32) pwm_config->frequency));

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

  foc_state->v_ab.alpha = app_setpoints.foc.voltageRef * cosVal;
  foc_state->v_ab.beta = app_setpoints.foc.voltageRef * sinVal;

  // Stores result straight into state
  focSVPWM(&foc_state->v_ab, &foc_state->duty_3ph);
}

static void focCurrentControlClosedLoop (float32 theta, const ThreePhaseCurrents *currents)
{
  float32 sinVal, cosVal;
  float32 theta_corr;
  float32 v_limit_id;
  float32 v_limit_iq;
  float32 v_limit;

  if ((foc_config == NULL_PTR) || (foc_state == NULL_PTR) || (currents == NULL_PTR))
  {
    return;
  }

  theta_corr = focGetMotorElecAngle(theta, foc_config->resolver_offset, foc_config->motor_polepairs);
  foc_state->electricalAngle = theta_corr;

  FOC_ClarkeTransform(currents, &foc_state->i_ab);

  sinVal = sinf(theta_corr);
  cosVal = cosf(theta_corr);
  FOC_ParkTransform(&foc_state->i_ab, &foc_state->i_dq, sinVal, cosVal);

  foc_state->v_dq.d = PI_Run(&foc_config->pi_config_id, &foc_state->pi_state_id, app_setpoints.foc.id_ref, foc_state->i_dq.d);
  foc_state->v_dq.q = PI_Run(&foc_config->pi_config_iq, &foc_state->pi_state_iq, app_setpoints.foc.iq_ref, foc_state->i_dq.q);
  v_limit_id = fmaxf(fabsf(foc_config->pi_config_id.outMax), fabsf(foc_config->pi_config_id.outMin));
  v_limit_iq = fmaxf(fabsf(foc_config->pi_config_iq.outMax), fabsf(foc_config->pi_config_iq.outMin));
  v_limit = fminf(v_limit_id, v_limit_iq);
  focLimitVoltageVector(&foc_state->v_dq, v_limit);

  FOC_InvParkTransform(&foc_state->v_dq, &foc_state->v_ab, sinVal, cosVal);
  // Stores result straight into state
  focSVPWM(&foc_state->v_ab, &foc_state->duty_3ph);
}

static void focCalibrateZeroOffset (float32 theta_measured)
{
  const float32 alignTheta = 0.0f;
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

  // Stores result straight into state
  focSVPWM(&foc_state->v_ab, &foc_state->duty_3ph);

  if ((current_time - starttime) > foc_config->calibration_ticks)
  {
    updateFocResolverOffset(theta_measured);
    foc_state->calibrated = true;
    starttime = 0U;
  }
}

static void focLimitVoltageVector(CurrentsDQ *v_dq, float32 limit)
{
  float32 magnitude;
  float32 scale;

  if ((v_dq == NULL_PTR) || (limit <= 0.0f))
  {
    return;
  }

  magnitude = sqrtf((v_dq->d * v_dq->d) + (v_dq->q * v_dq->q));
  if (magnitude <= limit)
  {
    return;
  }

  scale = limit / magnitude;
  v_dq->d *= scale;
  v_dq->q *= scale;
}
