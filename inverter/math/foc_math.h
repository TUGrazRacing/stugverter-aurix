#ifndef FOC_MATH_H_
#define FOC_MATH_H_

#include "Ifx_Types.h"
#include "current_math.h"
#include "foc_types.h"

/* Core Transformations */
void FOC_ClarkeTransform(const ThreePhaseCurrents *input_abc, CurrentsAB *output_ab);
void FOC_InvClarkeTransform(const CurrentsAB *input_ab, ThreePhaseCurrents *output_abc);
void FOC_ParkTransform(const CurrentsAB *input_ab, CurrentsDQ *output_dq, float32 sinTheta, float32 cosTheta);
void FOC_InvParkTransform(const CurrentsDQ *input_dq, CurrentsAB *output_ab, float32 sinTheta, float32 cosTheta);
float32 focWrapAngle(float32 angle);
float32 focGetMotorElecAngle(float32 theta_resolver, float32 resolver_offset, uint8 motor_polepairs);

/* Modulation */
void focSVPWM(const CurrentsAB *V_ab, ThreePhaseDuty *DutyCycles);

#endif /* FOC_MATH_H_ */
