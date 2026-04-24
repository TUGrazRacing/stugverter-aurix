#ifndef SPEED_H_
#define SPEED_H_

#include "Ifx_Types.h"
#include "types_config.h"
#include "types_state.h"

#define SPEED_CONTROL_LOOP_DIVIDER 10U

void Speed_Reset(FocState *state);
void Speed_UpdateMeasurement(FocState *state, float32 theta_mech_rad, float32 loop_hz);
void Speed_RunControlTask(const FocConfig *config,
                          FocSetpoints *setpoints,
                          FocState *state,
                          uint32 control_loop_counter);

#endif /* SPEED_H_ */
