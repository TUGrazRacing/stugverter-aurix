#include "foc_math.h"
#include <math.h>
#include "IfxCpu_Intrinsics.h"

void FOC_ClarkeTransform(const ThreePhaseCurrents *input_abc, CurrentsAB *output_ab)
{
    output_ab->alpha = input_abc->u;
    output_ab->beta  = FOC_ONE_BY_SQRT3 * (input_abc->u + (2.0f * input_abc->v));
}

void FOC_InvClarkeTransform(const CurrentsAB *input_ab, ThreePhaseCurrents *output_abc)
{
    float32 term_beta  = FOC_SQRT3_BY_2 * input_ab->beta;
    float32 term_alpha = -FOC_ONE_HALF * input_ab->alpha;

    output_abc->u = input_ab->alpha;
    output_abc->v = term_alpha + term_beta;
    output_abc->w = term_alpha - term_beta;
}

void FOC_ParkTransform(const CurrentsAB *input_ab, CurrentsDQ *output_dq, float32 sinTheta, float32 cosTheta)
{
    output_dq->d = (input_ab->alpha * cosTheta) + (input_ab->beta * sinTheta);
    output_dq->q = (-input_ab->alpha * sinTheta) + (input_ab->beta * cosTheta);
}

void FOC_InvParkTransform(const CurrentsDQ *input_dq, CurrentsAB *output_ab, float32 sinTheta, float32 cosTheta)
{
    output_ab->alpha = (input_dq->d * cosTheta) - (input_dq->q * sinTheta);
    output_ab->beta  = (input_dq->d * sinTheta) + (input_dq->q * cosTheta);
}

void focSVPWM(const CurrentsAB *V_ab, ThreePhaseDuty *DutyCycles)
{
    ThreePhaseCurrents V_phase_raw;
    float32 V_max, V_min;
    float32 V_offset;
    float32 V_a_mod, V_b_mod, V_c_mod;

    FOC_InvClarkeTransform(V_ab, &V_phase_raw);

    V_max = Ifx__maxf(Ifx__maxf(V_phase_raw.u, V_phase_raw.v), V_phase_raw.w);
    V_min = Ifx__minf(Ifx__minf(V_phase_raw.u, V_phase_raw.v), V_phase_raw.w);

    V_offset = -FOC_ONE_HALF * (V_max + V_min);

    V_a_mod = V_phase_raw.u + V_offset;
    V_b_mod = V_phase_raw.v + V_offset;
    V_c_mod = V_phase_raw.w + V_offset;

    DutyCycles->u = Ifx__saturatef((V_a_mod * 0.5f) + 0.5f, FOC_DUTY_MIN, FOC_DUTY_MAX);
    DutyCycles->v = Ifx__saturatef((V_b_mod * 0.5f) + 0.5f, FOC_DUTY_MIN, FOC_DUTY_MAX);
    DutyCycles->w = Ifx__saturatef((V_c_mod * 0.5f) + 0.5f, FOC_DUTY_MIN, FOC_DUTY_MAX);
}

float32 focWrapAngle(float32 angle)
{
    while (angle >= FOC_TWO_PI)
    {
        angle -= FOC_TWO_PI;
    }
    while (angle < 0.0f)
    {
        angle += FOC_TWO_PI;
    }
    return angle;
}

float32 focGetMotorElecAngle(float32 theta_resolver, float32 resolver_offset, uint8 motor_polepairs)
{
    return focWrapAngle((theta_resolver - resolver_offset) * (float32)motor_polepairs);
}
