#ifndef FOC_H
#define FOC_H

#include "Ifx_Types.h"
#include "foc_math.h"
#include "pi.h"
#include "foc_types.h"
#include "config.h"
#include "state.h"

void focInit(FocConfig *config, FocState *state);
void focStep(ThreePhaseDuty *dutycycles, float32 theta_resolver_mech, const ThreePhaseCurrents *currents);

#endif /* FOC_H */
