#ifndef PWM_H_
#define PWM_H_

#include <types_config.h>
#include "Ifx_Types.h"

#define PWM_FREQUENCY (20.0E3)

extern PwmConfig *pwm_config;

void pwmInit(PwmConfig *pconfig);
void setDutyCycles(float32 dutyU, float32 dutyV, float32 dutyW);

#endif /* PWM_H_ */
