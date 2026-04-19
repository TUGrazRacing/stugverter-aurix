/*
 * config.h
 *
 *  Created on: Mar 22, 2026
 *      Author: probstn
 */

#ifndef INVERTER_CONFIG_MODULES_CONFIG_H_
#define INVERTER_CONFIG_MODULES_CONFIG_H_

#include <Ifx_Types.h>
#include <stdbool.h>

typedef struct
{
  uint16 steps; //4096 = 12bit
  float supply; //5.0V
} AdcConfig;

typedef struct
{
  uint16 offset_u_adcsteps;
  uint16 offset_v_adcsteps;
  float32 current_sense_factor;
} CurrentConfig;

typedef struct
{
  float32 Kp;
  float32 Ki;
  float32 outMax;
  float32 outMin;
} PiConfig;

typedef struct
{
  uint8 motor_polepairs;

  uint64 calibration_ticks;
  float32 resolver_offset;

  PiConfig pi_config_id;
  PiConfig pi_config_iq;
} FocConfig;

typedef struct
{
  float32 speedRefHz;
  float32 voltageRef;
  float32 id_ref;
  float32 iq_ref;
} FocSetpoints;

typedef struct
{
  uint32 frequency;
} PwmConfig;

typedef struct
{
  uint16 sin_max;
  uint16 sin_min;
  uint16 cos_max;
  uint16 cos_min;
  uint16 offset_sin;
  uint16 offset_cos;

  uint8 pole_pairs;
} ResolverConfig;

#endif /* INVERTER_CONFIG_MODULES_CONFIG_H_ */
