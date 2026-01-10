/**********************************************************************************************************************
 * \file foc.h
 * \brief FOC Control Structures and Prototypes
 *********************************************************************************************************************/

#ifndef FOC_H_
#define FOC_H_

#include "Ifx_Types.h"

/* Open Loop FOC Settings (Constants) */
#define FOC_PWM_PERIOD          (1.0f / PWM_FREQUENCY)                 /* PWM Period in Seconds                      */
#define FOC_TWO_PI              (6.28318530718f)

typedef struct
{
    float32 d;
    float32 q;
} DQ_t;

typedef struct
{
    float32 alpha;
    float32 beta;
} AlphaBeta_t;

typedef struct
{
    float32 u;
    float32 v;
    float32 w;
} ThreePhase_t;

typedef struct
{
    float32       electricalAngle;    /* Current electrical angle [rad] */
    float32       speedRefHz;         /* Target speed [Hz] */
    float32       voltageRef;         /* Target voltage amplitude [0.0 - 1.0] */
    DQ_t          v_dq;               /* Voltage in DQ frame */
    AlphaBeta_t   v_ab;               /* Voltage in AlphaBeta frame */
    ThreePhase_t  duty_3ph;           /* Calculated Duty Cycles (0.0 - 1.0) */
} GtmFocControl;

/*********************************************************************************************************************/
/*-------------------------------------------------Global Variables--------------------------------------------------*/
/*********************************************************************************************************************/
/* These must be defined in main.c or foc.c, and externed here */
extern GtmFocControl g_focControl;

/*********************************************************************************************************************/
/*-----------------------------------------------Function Prototypes-------------------------------------------------*/
/*********************************************************************************************************************/
void initFoc(void);
void runCurrentControl(uint16 i_u_raw, uint16 i_v_raw);
void foc_openloop(void);

#endif /* FOC_H_ */
