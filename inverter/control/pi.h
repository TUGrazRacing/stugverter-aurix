#include "Ifx_Types.h"

#ifndef PI_H
#define PI_H

typedef struct
{
  float32 Kp;
  float32 Ki;

  float32 integral;

  float32 outMax;
  float32 outMin;

} PI_Controller_t;

void PI_Init(PI_Controller_t* pi, float32 kp, float32 ki, float32 min, float32 max);
float32 PI_Run(PI_Controller_t* pi, float32 setpoint, float32 current);

#endif
