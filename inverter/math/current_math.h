#ifndef CURRENT_MATH_H_
#define CURRENT_MATH_H_

#include "Ifx_Types.h"
#include "config.h"
#include "state.h"
#include "foc_types.h"

void currentsGet(ThreePhaseCurrents *currents, uint16 adc_u, uint16 adc_v);
void initCurrents(CurrentConfig *cconfig, AdcConfig *aconfig);

#endif /* CURRENT_MATH_H_ */
