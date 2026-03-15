#ifndef FOC_H
#define FOC_H

#include "Ifx_Types.h"
#include "foc_math.h"
#include "pi.h"
#include <stdbool.h>

#define FOC_PWM_FREQ            (20000.0f)
#define FOC_PWM_PERIOD          (1.0f / FOC_PWM_FREQ)

typedef struct
{
    float32         electricalAngle;
    float32         speedRefHz;
    float32         voltageRef;

    bool            calibrated;
    uint64          calibration_ticks;
    float32         resolver_offset;

    AlphaBeta_t     v_ab;
    ThreePhase_t    duty_3ph;

    AlphaBeta_t     i_ab;
    DQ_t            i_dq;
    DQ_t            v_dq;
    float32         id_ref;
    float32         iq_ref;

    PI_Controller_t pi_d;
    PI_Controller_t pi_q;

} FocControl;

extern FocControl foc;

void focInit(void);
void focRun(float32 theta_resolver_electrical, float32 iu, float32 iv);

#endif
