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
  uint16 steps;  /* ADC full-scale steps, e.g. 4096 for 12 bit. */
  float supply;  /* ADC reference voltage in V. */
} AdcConfig;

typedef struct
{
  uint16 offset_u_adcsteps;
  uint16 offset_v_adcsteps;
  float32 current_sense_factor; /* Current sensor transfer gain in V/A. */
  float32 filter_alpha;         /* 0..1, higher = faster response / less filtering. */
  float32 max_delta_a;          /* Per-sample current slew clamp in A, <=0 disables. */
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
  float32 speed_filter_alpha;             /* 0..1, speed estimate low-pass alpha. */
  float32 speed_setpoint_ramp_rpm_per_s;  /* Speed command ramp in rpm/s. */
  float32 iq_ref_slew_a_per_s;            /* Iq command slew in A/s. */

  /* Motor Parameters for Decoupling */
  float32 motor_rs;                       /* Stator resistance [Ohm] */
  float32 motor_ld;                       /* D-axis inductance [H] */
  float32 motor_lq;                       /* Q-axis inductance [H] */
  float32 motor_psi_pm;                   /* Permanent magnet flux linkage [Wb] */
  float32 motor_iron_loss_id_offset;      /* Iron loss compensation current [A], typically ~0 */
  bool    motor_decoupling_enable;        /* Enable voltage decoupling */

  PiConfig pi_config_id;
  PiConfig pi_config_iq;
  PiConfig pi_config_speed;
} FocConfig;

typedef enum
{
  CONTROL_MODE_CURRENT = 0U,
  CONTROL_MODE_SPEED   = 1U
} ControlMode;

typedef struct
{
  float32 speedSetpointRpm;
  float32 voltageRef;
  float32 id_ref;
  float32 iq_ref;
  uint8   controlMode;
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
