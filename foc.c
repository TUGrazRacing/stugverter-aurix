/**********************************************************************************************************************
 * \file foc.c
 * \brief Open Loop Control Implementation
 *********************************************************************************************************************/

#include "foc.h"
#include "pwm.h"
#include "IfxPort.h"
#include <math.h>

/*********************************************************************************************************************/
/*-------------------------------------------------Global Variables--------------------------------------------------*/
/*********************************************************************************************************************/

/* Define the control structure here */
GtmFocControl g_focControl;

/* ACCESSING EXTERNAL PWM DATA
 * Note: Go to pwm.c and remove "IFX_STATIC" from g_gtmAtom3phInv definition
 * so it is visible here.
 */
/* OR explicitly define the external struct if shared */
// extern GtmAtom3phInv g_gtmAtom3phInv;

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

void initFoc(void)
{
    /* Initialize default values for open loop */
    g_focControl.electricalAngle = 0.0f;
    g_focControl.speedRefHz      = 10.0f;  /* Start slow: 10Hz */
    g_focControl.voltageRef      = 0.10f;  /* Start weak: 10% Voltage */
}

/* * Main Control Loop
 * Triggered by ADC End-Of-Conversion Interrupt (100kHz)
 */
void runCurrentControl(uint16 i_u_raw, uint16 i_v_raw)
{
    /* 1. Profiling Start (Set Pin High) */
    /* Assuming P13.0 for LED/Debug, or use your specific debug pin */

    /* 2. Run Control Algorithm */
    foc_openloop();

    /* 3. Profiling End (Set Pin Low) */
}

void foc_openloop(void)
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

    /* --- 2. Calculate Reference Vector (Alpha/Beta) directly --- */
    /* In Open Loop, Vd=0. So V_alpha = Vref * cos(theta), V_beta = Vref * sin(theta) */
    /* Note: optimization - use a Look-Up Table (LUT) for sin/cos in 100kHz loop */
    sinVal = sinf(g_focControl.electricalAngle);
    cosVal = cosf(g_focControl.electricalAngle);

    g_focControl.v_ab.alpha = g_focControl.voltageRef * cosVal;
    g_focControl.v_ab.beta  = g_focControl.voltageRef * sinVal;

    /* --- 3. Simple Sine-PWM (Temporary replacement for full SVPWM) --- */
    /* This generates standard sine waves shifted by 120 degrees */
    /* Duty = 0.5 + (V_alpha_beta / 2) */

    /* Phase U (0 deg) -> Proportional to Alpha (Cos) */
    g_focControl.duty_3ph.u = 0.5f + (0.5f * g_focControl.v_ab.alpha);

    /* Phase V (-120 deg) -> -0.5*Alpha + 0.866*Beta */
    g_focControl.duty_3ph.v = 0.5f + (0.5f * ((-0.5f * g_focControl.v_ab.alpha) + (0.8660254f * g_focControl.v_ab.beta)));

    /* Phase W (+120 deg) -> -0.5*Alpha - 0.866*Beta */
    g_focControl.duty_3ph.w = 0.5f + (0.5f * ((-0.5f * g_focControl.v_ab.alpha) - (0.8660254f * g_focControl.v_ab.beta)));

    /* --- 4. Update Hardware --- */
    /* We use the function defined in pwm.c to update the global struct and registers */
    /* We need to pass these values to the PWM module.
     * Since we don't have direct access to the g_gtmAtom3phInv struct here without circular dependencies,
     * we will define a setter function in pwm.c usually.
     * FOR NOW: We assume we can access a setter helper function.
     */
    /*
    updateDutyCycles(g_focControl.duty_3ph.u * 100.0f,
                     g_focControl.duty_3ph.v * 100.0f,
                     g_focControl.duty_3ph.w * 100.0f);
    */
}
