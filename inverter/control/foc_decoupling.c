#include "foc_decoupling.h"
#include <math.h>

/**
 * @file foc_decoupling.c
 * @brief Industry-standard voltage decoupling for FOC control
 * 
 * Implements cross-coupling, back-EMF, and iron loss compensation
 */

/* Constants */
#define FOC_RPM_TO_RADS   (0.10471975512f)  /* 2*PI/60 */
#define FOC_MIN_OMEGA     (0.1f)            /* Minimum omega to apply back-EMF (prevent division issues) */

/**
 * @brief Apply voltage decoupling to the DQ reference voltages
 * 
 * The decoupling compensates for the interaction between d and q axes:
 *   v_d_decoupled = v_d + omega * L_q * i_q
 *   v_q_decoupled = v_q - omega * L_d * i_d - omega * Psi_pm
 * 
 * This ensures that the current controllers can independently regulate
 * id and iq without cross-coupling effects destabilizing the system.
 */
void focApplyVoltageDecoupling(
    float32 v_d,
    float32 v_q,
    float32 i_d,
    float32 i_q,
    float32 omega_elec,
    float32 ld,
    float32 lq,
    float32 psi_pm,
    float32 rs,
    VoltageDecoupled *v_decoupled
)
{
    float32 cross_coupling_d;
    float32 cross_coupling_q;
    float32 back_emf_q;
    float32 iron_loss_d;

    if (v_decoupled == NULL)
    {
        return;
    }

    /* D-axis cross-coupling: omega * L_q * i_q */
    cross_coupling_d = omega_elec * lq * i_q;

    /* Q-axis cross-coupling: omega * L_d * i_d */
    cross_coupling_q = omega_elec * ld * i_d;

    /* Q-axis back-EMF: omega * Psi_pm (only significant at high speed) */
    back_emf_q = (fabsf(omega_elec) > FOC_MIN_OMEGA) ? (omega_elec * psi_pm) : 0.0f;

    /* Optional iron loss compensation (negative d-current reduces iron losses) */
    /* This is typically a small offset, often 0 for normal operating modes */
    iron_loss_d = 0.0f;  /* Can be made parameter-dependent if needed */

    /* Apply decoupling */
    v_decoupled->d_voltage = v_d + cross_coupling_d + iron_loss_d;
    v_decoupled->q_voltage = v_q - cross_coupling_q - back_emf_q;
}

/**
 * @brief Apply iron loss compensation
 * 
 * At high speeds, injecting a small negative d-axis current (into the rotor)
 * can reduce iron losses in the stator and rotor. This is MTPA-adjacent
 * and improves efficiency.
 */
float32 focApplyIronLossCompensation(float32 i_d_ref, float32 i_d_offset)
{
    /* i_d_offset is typically negative (demagnetizing direction) */
    return i_d_ref + i_d_offset;
}

/**
 * @brief Calculate electrical angular velocity from mechanical speed
 * 
 * omega_elec = speed_mech * (2*PI/60) * pole_pairs
 */
float32 focCalcOmegaElec(float32 speed_mech_rpm, uint8 pole_pairs)
{
    return speed_mech_rpm * FOC_RPM_TO_RADS * (float32)pole_pairs;
}
