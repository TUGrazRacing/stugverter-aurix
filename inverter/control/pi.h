#ifndef PI_H_
#define PI_H_

#include "Ifx_Types.h"
#include "config.h"
#include "state.h"

void PI_Init(PiConfig *config, PiState *state, float32 kp, float32 ki, float32 min, float32 max);
float32 PI_Run(const PiConfig *config, PiState *state, float32 setpoint, float32 current);

#endif /* PI_H_ */
