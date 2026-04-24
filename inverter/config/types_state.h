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

    CurrentsAB     i_ab;
    CurrentsAB     v_ab;
    CurrentsDQ     i_dq;
    CurrentsDQ     v_dq;
    ThreePhaseDuty duty_3ph;

    PiState pi_state_id;
    PiState pi_state_iq;
    PiState pi_state_speed;

    float32 speed_mech_rpm;
    float32 speed_filt_rpm;
    float32 speed_iq_ref;
} FocState;

typedef struct
{
    float32 prev_elec_angle;
    sint8   sector;
} ResolverState;

#endif /* INVERTER_CONFIG_MODULES_STATE_H_ */
