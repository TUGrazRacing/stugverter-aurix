#ifndef INVERTER_MATH_FOC_TYPES_H_
#define INVERTER_MATH_FOC_TYPES_H_

#include <Ifx_Types.h>

/* Math Constants */
#define FOC_TWO_PI          (6.28318530718f)
#define FOC_SQRT3           (1.732050808f)
#define FOC_ONE_BY_SQRT3    (0.577350269f)
#define FOC_SQRT3_BY_2      (0.866025404f)
#define FOC_ONE_HALF        (0.5f)

/* Duty Cycle Limits */
#define FOC_DUTY_MAX        (1.0f)
#define FOC_DUTY_MIN        (0.0f)

typedef struct
{
    float32 alpha;
    float32 beta;
} CurrentsAB;

typedef struct
{
    float32 d;
    float32 q;
} CurrentsDQ;

typedef struct
{
    float32 u;
    float32 v;
    float32 w;
} ThreePhaseDuty;

typedef struct
{
    float32 u;
    float32 v;
    float32 w;
} ThreePhaseCurrents;

#endif /* INVERTER_MATH_FOC_TYPES_H_ */
