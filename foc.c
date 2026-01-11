/**********************************************************************************************************************
 * \file foc.c
 * \brief Open Loop Control Implementation using foc_math library
 *********************************************************************************************************************/

#include "foc.h"
#include "pwm.h"
#include "IfxPort.h"
#include "foc_math.h" /* Required for FOC_SVPWM */
#include <math.h>
#include <stdio.h>

/*********************************************************************************************************************/
/*-------------------------------------------------Global Variables--------------------------------------------------*/
/*********************************************************************************************************************/

/* Define the control structure here */
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

/* * Main Control Loop
 * Triggered by ADC End-Of-Conversion Interrupt (e.g. 20kHz or 100kHz)
 */
void focCurrentControl(uint16 i_u_raw, uint16 i_v_raw)
{
    /* 1. Profiling Start */
    /* IfxPort_setPinHigh(&MODULE_P13, 0); */

    /* 2. Run Control Algorithm */
    focOpenLoop();

    /* 3. Profiling End */
    /* IfxPort_setPinLow(&MODULE_P13, 0); */
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
     * * Note: for production code, use a lookup table (LUT) or CORDIC
     * instead of sinf/cosf to save CPU cycles.
     */
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
