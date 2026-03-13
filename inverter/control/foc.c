/**********************************************************************************************************************
 * \file foc.c
 * \brief Open Loop Control Implementation using foc_math library
 *********************************************************************************************************************/

#include "foc.h"
#include "phase_pwm.h"
#include "IfxPort.h"
#include "foc_math.h"
#include <math.h>
#include <stdio.h>
#include "pi.h"

/*********************************************************************************************************************/
/*-------------------------------------------------Global Variables--------------------------------------------------*/
/*********************************************************************************************************************/

GtmFocControl g_focControl;

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

void focInit(void)
{
    /* Initialize default values for open loop */
    g_focControl.electricalAngle = 0.0f;
    g_focControl.speedRefHz      = 30.0f;  /* Start slow: 10Hz */
    g_focControl.voltageRef      = 0.10f;  /* Start weak: 10% Voltage to prevent overcurrent */

    /* Zero out vectors */
    g_focControl.v_ab.alpha = 0.0f;
    g_focControl.v_ab.beta  = 0.0f;
    g_focControl.duty_3ph.a = 0.5f;
    g_focControl.duty_3ph.b = 0.5f;
    g_focControl.duty_3ph.c = 0.5f;
}

void focOpenLoop(void)
{
    float32 sinVal, cosVal;

    /* --- 1. Update Electrical Angle --- */
    /* Angle = Angle + (2 * PI * Freq * Period) */
    g_focControl.electricalAngle += (g_focControl.speedRefHz * FOC_TWO_PI * FOC_PWM_PERIOD);

    /* Wrap Angle (0 to 2PI) */
    if (g_focControl.electricalAngle >= FOC_TWO_PI)
    {
        g_focControl.electricalAngle -= FOC_TWO_PI;
    }
    else if (g_focControl.electricalAngle < 0.0f)
    {
        g_focControl.electricalAngle += FOC_TWO_PI;
    }
    /* --- 2. Calculate Reference Vector (Alpha/Beta) --- */
    /* * In Open Loop FOC, we simulate a rotating voltage vector.
     * V_alpha = Vref * cos(theta)
     * V_beta  = Vref * sin(theta)
     */
    //replace with IfxCpu_sinCosF32()
    sinVal = sinf(g_focControl.electricalAngle);
    cosVal = cosf(g_focControl.electricalAngle);

    g_focControl.v_ab.alpha = g_focControl.voltageRef * cosVal;
    g_focControl.v_ab.beta  = g_focControl.voltageRef * sinVal;

    /* --- 3. Space Vector Modulation (SVPWM) --- */
    /* * Use the foc_math library to calculate the duty cycles with
     * Zero Sequence Injection (Min-Max) for better bus utilization.
     */
    focSVPWM(&g_focControl.v_ab, &g_focControl.duty_3ph);

    /* --- 4. Update Hardware --- */
    /* * Pass the calculated duty cycles to the GTM PWM module.
     * FOC_SVPWM returns 0.0 to 1.0, updateDutyCycles expects 0.0 to 100.0
     */
    setDutyCycles(g_focControl.duty_3ph.a * 100.0f,
                         g_focControl.duty_3ph.b * 100.0f,
                         g_focControl.duty_3ph.c * 100.0f);
}

static inline focCurrentControlClosedLoop(float32 theta, float32 iu, float32 iv)
{
    float32 sinVal, cosVal;

    /* 1. Clarke Transform (abc -> alpha beta) */
    ThreePhase_t i_abc;

    i_abc.a = iu;
    i_abc.b = iv;
    i_abc.c = -(iu + iv);

    FOC_ClarkeTransform(&i_abc, &g_focControl.i_ab);


    /* 2. Park Transform (alpha beta -> dq) */
    sinVal = sinf(theta);
    cosVal = cosf(theta);
    FOC_ParkTransform(&g_focControl.i_ab, sinVal, cosVal, &g_focControl.i_dq);

    /* 3. Current Control (PI) */
    g_focControl.v_dq.d = PI_Run(&g_focControl.pi_d, g_focControl.id_ref, g_focControl.i_dq.d);
    g_focControl.v_dq.q = PI_Run(&g_focControl.pi_q, g_focControl.iq_ref, g_focControl.i_dq.q);

    /* 4. Inverse Park (dq -> alpha beta) */
    focInvPark(&g_focControl.v_dq, sinVal, cosVal, &g_focControl.v_ab);

    /* 5. SVPWM */
    focSVPWM(&g_focControl.v_ab, &g_focControl.duty_3ph);

    /* 6. Update PWM */
    setDutyCycles(g_focControl.duty_3ph.a * 100.0f,
                  g_focControl.duty_3ph.b * 100.0f,
                  g_focControl.duty_3ph.c * 100.0f);
}
