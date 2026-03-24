#ifndef FOC_H
#define FOC_H

#include <types_config.h>
#include <types_state.h>
#include "Ifx_Types.h"
#include "foc_math.h"
#include "pi.h"
#include "foc_types.h"

void focInit(FocConfig *config, FocState *state);
void focStep(ThreePhaseDuty *dutycycles, float32 theta_resolver_mech, const ThreePhaseCurrents *currents);

#endif /* FOC_H */
