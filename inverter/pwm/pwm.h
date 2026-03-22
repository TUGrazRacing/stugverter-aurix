#ifndef PWM_H_
#define PWM_H_

#include "Ifx_Types.h"
#include "config.h"

#define PWM_FREQUENCY (20.0E3)

extern PwmConfig *pwm_config;

void pwmInit(PwmConfig *pconfig);
void setDutyCycles(float32 dutyU, float32 dutyV, float32 dutyW);

#endif /* PWM_H_ */
