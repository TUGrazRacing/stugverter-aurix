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
#include <stdbool.h>

/*********************************************************************************************************************/
/*-------------------------------------------------Global Variables--------------------------------------------------*/
/*********************************************************************************************************************/
#define FOC_MOTOR_POLE_PAIRS      (7.0f)
#define FOC_SENSOR_TO_MOTOR_RATIO (FOC_MOTOR_POLE_PAIRS / FOC_RESOLVER_POLE_PAIRS)

FocControl foc;

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

float32 focGetMotorElecAngle(float32 theta_resolver);
void focCalibrateZeroOffset(float32 theta_measured);
void focCurrentControlClosedLoop(float32 theta, float32 iu, float32 iv);
void focOpenLoop(void);

void focInit(void)
{
    foc.electricalAngle = 0.0f;
    foc.speedRefHz      = 15.0f;
    foc.voltageRef      = 0.125f;

    foc.resolver_offset = 0.0f;
    foc.calibration_ticks = (uint64)IfxStm_getFrequency(&MODULE_STM0) * 5ULL;
    foc.calibrated = FALSE;

    foc.v_ab.alpha = 0.0f;
    foc.v_ab.beta  = 0.0f;

    foc.duty_3ph.u = 0.0f;
    foc.duty_3ph.v = 0.0f;
    foc.duty_3ph.w = 0.0f;

    foc.id_ref = 0.0f;
    foc.iq_ref = 0.01f;   // nonzero torque command for testing

    PI_Init(&foc.pi_d, 0.2f, 0.001f, -0.5f, 0.5f);
    PI_Init(&foc.pi_q, 0.2f, 0.001f, -0.5f, 0.5f);
}

void focRun(float32 theta_resolver_mech, float32 iu, float32 iv)
{
    float32 sinVal, cosVal;
    float32 theta_corr;
    ThreePhase_t i_uvw;

    if(foc.calibrated)
    {
        /* 1. Get the electrical angle from the sensor */
        theta_corr = focGetMotorElecAngle(theta_resolver_mech);

        /* 2. Map the phase currents */
        i_uvw.u = iu;
        i_uvw.v = iv;
        i_uvw.w = -(iu + iv);

        /* 3. Execute Clarke Transform (abc -> alpha/beta) */
        FOC_ClarkeTransform(&i_uvw, &foc.i_ab);

        /* 4. Execute Park Transform (alpha/beta -> dq) */
        sinVal = sinf(theta_corr);
        cosVal = cosf(theta_corr);
        FOC_ParkTransform(&foc.i_ab, &foc.i_dq, sinVal, cosVal);

        /* 5. Log measured Id, measured Iq, and the electrical angle */
        logPush(&(LogData_t){iu, iv, theta_corr});

        /* 6. Run Open Loop Control (applies V/f voltage, ignores the currents above) */
        focOpenLoop();
    }
    else
    {
        focCalibrateZeroOffset(theta_resolver_mech);
    }
}

void focOpenLoop(void)
{
    float32 sinVal, cosVal;

    /* --- 1. Update Electrical Angle --- */
    /* Angle = Angle + (2 * PI * Freq * Period) */
    foc.electricalAngle += (foc.speedRefHz * FOC_TWO_PI * FOC_PWM_PERIOD);

    /* Wrap Angle (0 to 2PI) */
    if (foc.electricalAngle >= FOC_TWO_PI)
    {
        foc.electricalAngle -= FOC_TWO_PI;
    }
    else if (foc.electricalAngle < 0.0f)
    {
        foc.electricalAngle += FOC_TWO_PI;
    }

    /* --- 2. Calculate Reference Vector (Alpha/Beta) --- */
    /* Note: IfxCpu_sinCosF32() or Ifx_Math_SinCosF32 can be used here
       if you include the proper Infineon iLLD math headers */
    sinVal = sinf(foc.electricalAngle);
    cosVal = cosf(foc.electricalAngle);

    foc.v_ab.alpha = foc.voltageRef * cosVal;
    foc.v_ab.beta  = foc.voltageRef * sinVal;

    /* --- 3. Space Vector Modulation (SVPWM) --- */
    focSVPWM(&foc.v_ab, &foc.duty_3ph);

    /* --- 4. Update Hardware --- */
    setDutyCycles(foc.duty_3ph.u * 100.0f,
                  foc.duty_3ph.v * 100.0f,
                  foc.duty_3ph.w * 100.0f);
}

void focCurrentControlClosedLoop(float32 theta, float32 iu, float32 iv)
{
    float32 sinVal, cosVal;
    float32 theta_corr;
    ThreePhase_t i_abc;

    /* Apply resolver electrical zero offset */
    theta_corr = focGetMotorElecAngle(theta);

    /* 1. Clarke Transform (abc -> alpha beta) */
    i_abc.u = iu;
    i_abc.v = iv;
    i_abc.w = -(iu + iv);

    FOC_ClarkeTransform(&i_abc, &foc.i_ab);

    /* 2. Park Transform (alpha beta -> dq) */
    sinVal = sinf(theta_corr);
    cosVal = cosf(theta_corr);
    FOC_ParkTransform(&foc.i_ab, &foc.i_dq, sinVal, cosVal);

    /* 3. Current Control (PI) */
    foc.v_dq.d = PI_Run(&foc.pi_d, foc.id_ref, foc.i_dq.d);
    foc.v_dq.q = PI_Run(&foc.pi_q, foc.iq_ref, foc.i_dq.q);

    logPush(&(LogData_t){foc.i_dq.d, foc.i_dq.q, theta});

    /* 5. Inverse Park (dq -> alpha beta) */
    FOC_InvParkTransform(&foc.v_dq, &foc.v_ab, sinVal, cosVal);

    /* 6. SVPWM */
    focSVPWM(&foc.v_ab, &foc.duty_3ph);

    /* 7. Update PWM */
    setDutyCycles(foc.duty_3ph.u * 100.0f,
                  foc.duty_3ph.v * 100.0f,
                  foc.duty_3ph.w * 100.0f);
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
    theta_corr = focGetMotorElecAngle(theta);

    /* 1. Step reference generation */
    foc.id_ref = 0.0f;

    if (isrCount < stepDelayTicks)
    {
        foc.iq_ref = iqStepLow;
    }
    else
    {
        foc.iq_ref = iqStepHigh;
    }

    isrCount++;

    /* 2. Clarke transform */
    i_abc.u = iu;
    i_abc.v = iv;
    i_abc.w = -(iu + iv);

    FOC_ClarkeTransform(&i_abc, &foc.i_ab);

    /* 3. Park transform */
    sinVal = sinf(theta_corr);
    cosVal = cosf(theta_corr);
    FOC_ParkTransform(&foc.i_ab, &foc.i_dq, sinVal, cosVal);

    /* 4. PI current controllers */
    foc.v_dq.d = PI_Run(&foc.pi_d,
                                 foc.id_ref,
                                 foc.i_dq.d);

    foc.v_dq.q = PI_Run(&foc.pi_q,
                                 foc.iq_ref,
                                 foc.i_dq.q);

    /* 5. Log for step response visualization
       field1 = iq_ref setpoint
       field2 = measured iq
       field3 = measured id
    */
    logPush(&(LogData_t){
        foc.iq_ref,
        foc.v_dq.q,
        foc.i_dq.q
    });

    /* 6. Inverse Park */
    FOC_InvParkTransform(&foc.v_dq, &foc.v_ab, sinVal, cosVal);

    /* 7. SVPWM */
    focSVPWM(&foc.v_ab, &foc.duty_3ph);

    /* 8. Update PWM */
    setDutyCycles(foc.duty_3ph.u * 100.0f,
                  foc.duty_3ph.v * 100.0f,
                  foc.duty_3ph.w * 100.0f);
}

float32 focGetMotorElecAngle(float32 theta_resolver)
{
    return focWrapAngle((theta_resolver-foc.resolver_offset) * FOC_MOTOR_POLE_PAIRS);
}

void focCalibrateZeroOffset(float32 theta_measured)
{
    const float32 alignTheta   = 0.0f;   /* motor electrical angle we want to lock to */
    const float32 alignVoltage = 0.15f;  /* increase carefully if needed */

    float32 sinVal, cosVal;

    /* Fix 1: Correct static initialization for the timer */
    static uint64 starttime = 0;
    uint64 current_time = IfxStm_get(&MODULE_STM0);

    if (starttime == 0)
    {
        starttime = current_time;
    }

    /* 1. Apply fixed d-axis alignment voltage */
    foc.v_dq.d = alignVoltage;
    foc.v_dq.q = 0.0f;

    sinVal = sinf(alignTheta);
    cosVal = cosf(alignTheta);

    FOC_InvParkTransform(&foc.v_dq, &foc.v_ab, sinVal, cosVal);

    focSVPWM(&foc.v_ab, &foc.duty_3ph);

    setDutyCycles(foc.duty_3ph.u * 100.0f,
                  foc.duty_3ph.v * 100.0f,
                  foc.duty_3ph.w * 100.0f);

    /* 2. Wait for rotor to physically settle at motor electrical zero */
    if(current_time - starttime > foc.calibration_ticks)
    {
        /* Fix 2: Zero out the unwrapper AT THIS PHYSICAL POSITION.
         * By setting resolver_unwrapped to 0.0f here, we guarantee that
         * (0.0f * 1.75) = 0.0f motor electrical angle.
         */
        foc.resolver_offset = theta_measured;
        foc.calibrated = true;
        starttime = 0;

        /* Log raw sensor angle and the newly zeroed state */
        logPush(&(LogData_t){
            theta_measured, /* Raw resolver position at lock */
            focGetMotorElecAngle(theta_measured),           /* Unwrapped is now exactly 0 */
            0.0f,
        });
    }
}
