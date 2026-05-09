#ifndef GTM_H_
#define GTM_H_

#include "Ifx_Types.h"

#include "gtm/gtm_atom.h"
#include "gtm/gtm_tim.h"

/**
 * @brief Initializes shared GTM resources used by ATOM and TIM modules.
 */
void gtmInit(void);

/**
 * @brief Enables GTM and CMU clock 0 if it is not already running.
 */
void gtmEnableClock0(void);

#endif /* GTM_H_ */
