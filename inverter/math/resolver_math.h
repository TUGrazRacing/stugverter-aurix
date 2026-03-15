#ifndef RESOLVER_MATH_H
#define RESOLVER_MATH_H

#include "Ifx_Types.h"

/* * Structure to hold calibration and state data.
 * Keeps variables context-specific so you can support multiple resolvers.
 */
typedef struct {
    uint16  sin_max;
    uint16  sin_min;
    uint16  cos_max;
    uint16  cos_min;
    uint16  offset_sin;
    uint16  offset_cos;

    float32 prev_elec_angle;
    sint8   sector;
    uint8   pole_pairs;
} Resolver_Config_t;

/* Function prototypes */
void resolverInit(Resolver_Config_t* config);
void resolverUpdateCalibration(Resolver_Config_t* config, uint16 sin_val, uint16 cos_val);
float32 resolverGetMechanicalAngle(Resolver_Config_t* config, uint16 sin_val, uint16 cos_val);

#endif /* RESOLVER_MATH_H */
