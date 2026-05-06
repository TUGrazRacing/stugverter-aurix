#ifndef INVERTER_CONFIG_MODULES_STATE_H_
#define INVERTER_CONFIG_MODULES_STATE_H_

#include "foc_types.h"
#include "Ifx_Types.h"
#include <stdbool.h>

typedef struct
{
    float32 integral;
} PiState;

typedef struct
{
    float32 electricalAngle;
    bool    calibrated;

    uint16 adc_curr_u_raw;
    uint16 adc_curr_v_raw;
    uint16 adc_sin_raw;
    uint16 adc_cos_raw;
    uint16 adc_curr_u;
    uint16 adc_curr_v;
    uint16 adc_curr_w;

    ThreePhaseCurrents i_uvw;
    CurrentsAB     i_ab;
    CurrentsAB     v_ab;
    CurrentsDQ     i_dq;
    CurrentsDQ     v_dq;
    CurrentsDQ     v_dq_decoupled;  /* Voltage after decoupling compensation */
    ThreePhaseDuty duty_3ph;

    PiState pi_state_id;
    PiState pi_state_iq;
    PiState pi_state_speed;

    float32 speed_mech_rpm;
    float32 speed_iq_ref;
    float32 resolver_mech_angle;
    float32 omega_elec;                 /* Electrical angular velocity [rad/s] for decoupling */
    uint32  control_loop_counter;
    uint64  control_loop_tick;
} FocState;

typedef struct
{
    float32 prev_elec_angle;
    sint8   sector;
} ResolverState;

#endif /* INVERTER_CONFIG_MODULES_STATE_H_ */
