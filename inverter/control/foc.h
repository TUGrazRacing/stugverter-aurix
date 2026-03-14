#ifndef FOC_H
#define FOC_H

#include "Ifx_Types.h"
#include "foc_math.h"
#include "pi.h"

#define FOC_PWM_FREQ            (20000.0f)
#define FOC_PWM_PERIOD          (1.0f / FOC_PWM_FREQ)
#define FOC_TWO_PI              (6.28318530718f)

typedef struct
{
    float32         electricalAngle;
    float32         speedRefHz;
    float32         voltageRef;

    float32         angle_offset;

    AlphaBeta_t     v_ab;
    ThreePhase_t    duty_3ph;

    AlphaBeta_t     i_ab;
    DQ_t            i_dq;
    DQ_t            v_dq;
    float32         id_ref;
    float32         iq_ref;

    PI_Controller_t pi_d;
    PI_Controller_t pi_q;

} GtmFocControl;

extern GtmFocControl g_focControl;

void focInit(void);
void focOpenLoop(void);
void focCurrentControlClosedLoop(float32 theta, float32 iu, float32 iv);
void focCurrentControlPiStepTest(float32 theta, float32 iu, float32 iv);
void focGetZeroOffset(float32 theta_measured);

#endif
