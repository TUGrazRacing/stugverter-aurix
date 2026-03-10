#ifndef RESOLVER_MATH_H
#define RESOLVER_MATH_H

#include "Ifx_Types.h" // Gives us float32, uint16, etc.

/* Initialization function */
void Resolver_Math_Init(Resolver_Config_t* config, uint16 zero_offset_sin, uint16 zero_offset_cos);

/* The high-speed ISR calculation function */
float32 Resolver_Math_GetAngle(const Resolver_Config_t* config, uint16 sin_val, uint16 cos_val);

#endif /* RESOLVER_MATH_H */
