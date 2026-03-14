/**********************************************************************************************************************
 * \file foc.c
 * \brief FOC Open and Closed Loop Control Implementation
 *********************************************************************************************************************/

#include "foc.h"
#include "phase_pwm.h"
#include "IfxPort.h"
#include "foc_math.h"
#include "pi.h"
#include <math.h>
#include "logger.h"

/*********************************************************************************************************************/
/*-------------------------------------------------Global Variables--------------------------------------------------*/
/*********************************************************************************************************************/

GtmFocControl g_focControl;

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

void focInit(void)
{
    g_focControl.electricalAngle = 0.0f;
    g_focControl.speedRefHz      = 20.0f;
    g_focControl.voltageRef      = 0.007f;

    g_focControl.angle_offset    = 2.485f;

    g_focControl.v_ab.alpha = 0.0f;
    g_focControl.v_ab.beta  = 0.0f;

    g_focControl.duty_3ph.a = 0.0f;
    g_focControl.duty_3ph.b = 0.0f;
    g_focControl.duty_3ph.c = 0.0f;

    g_focControl.id_ref = 0.0f;
    g_focControl.iq_ref = 1.0f;   // nonzero torque command for testing

    PI_Init(&g_focControl.pi_d, 0.2f, 0.001f, -0.5f, 0.5f);
    PI_Init(&g_focControl.pi_q, 0.2f, 0.001f, -0.5f, 0.5f);
}

static float32 wrapAngle_0_2pi(float32 angle)
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
    /* Note: IfxCpu_sinCosF32() or Ifx_Math_SinCosF32 can be used here
       if you include the proper Infineon iLLD math headers */
    sinVal = sinf(g_focControl.electricalAngle);
    cosVal = cosf(g_focControl.electricalAngle);

    g_focControl.v_ab.alpha = g_focControl.voltageRef * cosVal;
    g_focControl.v_ab.beta  = g_focControl.voltageRef * sinVal;

    /* --- 3. Space Vector Modulation (SVPWM) --- */
    focSVPWM(&g_focControl.v_ab, &g_focControl.duty_3ph);

    /* --- 4. Update Hardware --- */
    setDutyCycles(g_focControl.duty_3ph.a * 100.0f,
                  g_focControl.duty_3ph.b * 100.0f,
                  g_focControl.duty_3ph.c * 100.0f);
}

void focCurrentControlClosedLoop(float32 theta, float32 iu, float32 iv)
{
    float32 sinVal, cosVal;
    float32 theta_corr;
    ThreePhase_t i_abc;

    /* Apply resolver electrical zero offset */
    theta_corr = wrapAngle_0_2pi(theta - g_focControl.angle_offset);

    /* 1. Clarke Transform (abc -> alpha beta) */
    i_abc.a = iu;
    i_abc.b = iv;
    i_abc.c = -(iu + iv);

    FOC_ClarkeTransform(&i_abc, &g_focControl.i_ab);

    /* 2. Park Transform (alpha beta -> dq) */
    sinVal = sinf(theta_corr);
    cosVal = cosf(theta_corr);
    FOC_ParkTransform(&g_focControl.i_ab, &g_focControl.i_dq, sinVal, cosVal);

    /* 3. Current Control (PI) */
    g_focControl.v_dq.d = PI_Run(&g_focControl.pi_d, g_focControl.id_ref, g_focControl.i_dq.d);
    g_focControl.v_dq.q = PI_Run(&g_focControl.pi_q, g_focControl.iq_ref, g_focControl.i_dq.q);

    logPush(&(LogData_t){
        theta_corr,
        g_focControl.i_dq.d,
        g_focControl.i_dq.q
    });

    /* 5. Inverse Park (dq -> alpha beta) */
    FOC_InvParkTransform(&g_focControl.v_dq, &g_focControl.v_ab, sinVal, cosVal);

    /* 6. SVPWM */
    focSVPWM(&g_focControl.v_ab, &g_focControl.duty_3ph);

    /* 7. Update PWM */
    setDutyCycles(g_focControl.duty_3ph.a * 100.0f,
                  g_focControl.duty_3ph.b * 100.0f,
                  g_focControl.duty_3ph.c * 100.0f);
}

void focCurrentControlPiStepTest(float32 theta, float32 iu, float32 iv)
{
    float32 sinVal, cosVal;
    float32 theta_corr;
    ThreePhase_t i_abc;

    /* persistent test state */
    static uint32 isrCount = 0U;

    /* tune these for your setup */
    const uint32 stepDelayTicks = 2000U;   /* 2000 * 50us = 100ms at 20kHz */
    const float32 iqStepLow     = 0.0f;
    const float32 iqStepHigh    = 0.2f;    /* start small for safety */

    /* 0. Apply resolver offset */
    theta_corr = wrapAngle_0_2pi(theta - g_focControl.angle_offset);

    /* 1. Step reference generation */
    g_focControl.id_ref = 0.0f;

    if (isrCount < stepDelayTicks)
    {
        g_focControl.iq_ref = iqStepLow;
    }
    else
    {
        g_focControl.iq_ref = iqStepHigh;
    }

    isrCount++;

    /* 2. Clarke transform */
    i_abc.a = iu;
    i_abc.b = iv;
    i_abc.c = -(iu + iv);

    FOC_ClarkeTransform(&i_abc, &g_focControl.i_ab);

    /* 3. Park transform */
    sinVal = sinf(theta_corr);
    cosVal = cosf(theta_corr);
    FOC_ParkTransform(&g_focControl.i_ab, &g_focControl.i_dq, sinVal, cosVal);

    /* 4. PI current controllers */
    g_focControl.v_dq.d = PI_Run(&g_focControl.pi_d,
                                 g_focControl.id_ref,
                                 g_focControl.i_dq.d);

    g_focControl.v_dq.q = PI_Run(&g_focControl.pi_q,
                                 g_focControl.iq_ref,
                                 g_focControl.i_dq.q);

    /* 5. Log for step response visualization
       field1 = iq_ref setpoint
       field2 = measured iq
       field3 = measured id
    */
    logPush(&(LogData_t){
        g_focControl.iq_ref,
        g_focControl.v_dq.q,
        g_focControl.i_dq.q
    });

    /* 6. Inverse Park */
    FOC_InvParkTransform(&g_focControl.v_dq, &g_focControl.v_ab, sinVal, cosVal);

    /* 7. SVPWM */
    focSVPWM(&g_focControl.v_ab, &g_focControl.duty_3ph);

    /* 8. Update PWM */
    setDutyCycles(g_focControl.duty_3ph.a * 100.0f,
                  g_focControl.duty_3ph.b * 100.0f,
                  g_focControl.duty_3ph.c * 100.0f);
}

void focGetZeroOffset(float32 theta_measured)
{
    float32 alignTheta = 0.0f;    /* electrical angle to lock to */
    float32 alignVoltage = 0.002f; /* increase a bit if rotor does not hold */
    float32 sinVal;
    float32 cosVal;

    /* Fixed d-axis alignment voltage, no torque command */
    g_focControl.v_dq.d = alignVoltage;
    g_focControl.v_dq.q = 0.0f;

    /* Lock rotor to a fixed electrical angle */
    sinVal = sinf(alignTheta);
    cosVal = cosf(alignTheta);

    FOC_InvParkTransform(&g_focControl.v_dq, &g_focControl.v_ab, sinVal, cosVal);

    /* Generate PWM */
    focSVPWM(&g_focControl.v_ab, &g_focControl.duty_3ph);

    setDutyCycles(g_focControl.duty_3ph.a * 100.0f,
                  g_focControl.duty_3ph.b * 100.0f,
                  g_focControl.duty_3ph.c * 100.0f);

    /* Continuously log measured resolver angle */
    logPush(&(LogData_t){
        theta_measured,
        0.0f,
        0.0f
    });
}

