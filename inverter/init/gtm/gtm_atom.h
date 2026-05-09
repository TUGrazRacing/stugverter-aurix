#ifndef GTM_ATOM_H_
#define GTM_ATOM_H_

#include <types_config.h>
#include "Ifx_Types.h"

#define PWM_FREQUENCY (20.0E3)

extern PwmConfig *pwm_config;

/**
 * @brief Initializes the three-phase ATOM PWM and ADC trigger output.
 */
void pwmInit(PwmConfig *pconfig);

/**
 * @brief Updates the three phase PWM duty-cycle commands in percent.
 */
void setDutyCycles(float32 dutyU, float32 dutyV, float32 dutyW);

#endif /* GTM_ATOM_H_ */
