#ifndef FOC_DECOUPLING_H
#define FOC_DECOUPLING_H

#include <Ifx_Types.h>

/**
 * @file foc_decoupling.h
 * @brief Industry-standard voltage decoupling for FOC control
 * 
 * Implements:
 * - Voltage feedforward compensation
 * - Cross-coupling compensation (d-q axis interaction)
 * - Back-EMF compensation
 * - Iron loss compensation (optional)
 */

typedef struct
{
    float32 d_voltage;  /* Decoupled d-axis voltage */
    float32 q_voltage;  /* Decoupled q-axis voltage */
} VoltageDecoupled;

/**
 * @brief Apply voltage decoupling to the DQ reference voltages
 * 
 * Implements the decoupling equations:
 *   v_d_decoupled = v_d + omega * L_q * i_q (cross-coupling)
 *   v_q_decoupled = v_q - omega * L_d * i_d - omega * Psi_pm (back-EMF)
 * 
 * @param[in] v_d           Desired d-axis voltage [V]
 * @param[in] v_q           Desired q-axis voltage [V]
 * @param[in] i_d           Measured d-axis current [A]
 * @param[in] i_q           Measured q-axis current [A]
 * @param[in] omega_elec    Electrical angular velocity [rad/s]
 * @param[in] ld            D-axis inductance [H]
 * @param[in] lq            Q-axis inductance [H]
 * @param[in] psi_pm        Permanent magnet flux linkage [Wb]
 * @param[in] rs            Stator resistance [Ohm] - for iron loss compensation
 * @param[out] v_decoupled  Decoupled voltage output
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
);

/**
 * @brief Apply iron loss compensation
 * 
 * Adds a small d-axis current bias to reduce iron losses at high speeds
 * i_d_compensated = i_d - i_d_offset (iron loss)
 * 
 * @param[in] i_d_ref       Original d-axis current reference [A]
 * @param[in] i_d_offset    Iron loss current offset [A]
 * @return                  Compensated d-axis current reference [A]
 */
float32 focApplyIronLossCompensation(float32 i_d_ref, float32 i_d_offset);

/**
 * @brief Calculate electrical angular velocity from speed and pole pairs
 * 
 * @param[in] speed_mech_rpm    Mechanical speed [RPM]
 * @param[in] pole_pairs        Motor pole pairs
 * @return                      Electrical angular velocity [rad/s]
 */
float32 focCalcOmegaElec(float32 speed_mech_rpm, uint8 pole_pairs);

#endif /* FOC_DECOUPLING_H */
