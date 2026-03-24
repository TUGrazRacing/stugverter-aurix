#ifndef RESOLVER_MATH_H
#define RESOLVER_MATH_H

#include <types_config.h>
#include <types_state.h>
#include "Ifx_Types.h"

void resolverInit(ResolverConfig *config, ResolverState *state);
void resolverUpdateCalibration(ResolverConfig *config, uint16 sin_val, uint16 cos_val);
float32 resolverGetMechanicalAngle(uint16 sin_val, uint16 cos_val);
#endif /* RESOLVER_MATH_H */
