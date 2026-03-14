#include "pi.h"

void PI_Init(PI_Controller_t* pi, float32 kp, float32 ki, float32 min, float32 max)
{
  pi->Kp = kp;
  pi->Ki = ki;

  pi->integral = 0.0f;

  pi->outMax = max;
  pi->outMin = min;
}

float32 PI_Run(PI_Controller_t* pi, float32 setpoint, float32 current)
{
  float32 error;
  float32 output;

  /* Error */
  error = setpoint - current;

  /* Integrator */
  pi->integral += pi->Ki * error;

  /* PI Output */
  output = (pi->Kp * error) + pi->integral;

  /* Saturation + Anti-windup */
  if (output > pi->outMax)
  {
    output = pi->outMax;
    pi->integral -= pi->Ki * error;
  }
  else if (output < pi->outMin)
  {
    output = pi->outMin;
    pi->integral -= pi->Ki * error;
  }

  return output;
}
