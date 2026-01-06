/**********************************************************************************************************************
 * \file math.c
 * \copyright Copyright (C) Infineon Technologies AG 2019
 * Niklas Probst
 * Note: To get a full understanding of all the math involved in pmsm commutation watch this video series
 * https://www.youtube.com/watch?v=EHYEQM1sA3o&list=PLaBr_WzeIAixidGwqfcrQlwKZX4RZ2E7D
 * Especially interesting for foc math is the clark park transform video in the series
 *********************************************************************************************************************/

#include "foc_math.h"
#include <math.h> /* For standard math functions if needed, though mostly manual implementation is used below */
#include "IfxCpu_Intrinsics.h" //aurix instructions
#include "stdio.h"

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief Performs the Clarke Transformation (abc → αβ).
 *
 * Converts three-phase stationary frame quantities into
 * two-phase stationary orthogonal components.
 *
 * The transformation assumes a balanced three-phase system:
 *   α = Ia
 *   β = (Ia + 2·Ib) / √3
 *
 * @param[in]  input_abc  Pointer to three-phase input structure (a, b, c)
 * @param[out] output_ab  Pointer to αβ output structure
 */
void FOC_ClarkeTransform(const ThreePhase_t *input_abc, AlphaBeta_t *output_ab)
{
    /*
     * Alpha = Ia
     * Beta  = (Ia + 2*Ib) / sqrt(3)
     */
    output_ab->alpha = input_abc->a;
    output_ab->beta  = FOC_ONE_BY_SQRT3 * (input_abc->a + (2.0f * input_abc->b));
}

/**
 * @brief Performs the Inverse Clarke Transformation (αβ → abc).
 *
 * Converts stationary αβ components back into three-phase
 * stationary quantities.
 *
 * The inverse equations are:
 *   Va = Valpha
 *   Vb = -0.5·Valpha + (√3/2)·Vbeta
 *   Vc = -0.5·Valpha - (√3/2)·Vbeta
 *
 * @param[in]  input_ab   Pointer to αβ input structure
 * @param[out] output_abc Pointer to three-phase output structure (a, b, c)
 */
void FOC_InvClarkeTransform(const AlphaBeta_t *input_ab, ThreePhase_t *output_abc)
{
    /*
     * Va = Valpha
     * Vb = -0.5 * Valpha + (sqrt(3)/2) * Vbeta
     * Vc = -0.5 * Valpha - (sqrt(3)/2) * Vbeta
     */
    float32 term_beta = FOC_SQRT3_BY_2 * input_ab->beta;
    float32 term_alpha = -FOC_ONE_HALF * input_ab->alpha;

    output_abc->a = input_ab->alpha;
    output_abc->b = term_alpha + term_beta;
    output_abc->c = term_alpha - term_beta;
}

/**
 * @brief Performs the Park Transformation (αβ → dq).
 *
 * Rotates stationary αβ components into the rotating dq reference frame
 * using the electrical angle θ.
 *
 * This transform aligns:
 *   - d-axis with the rotor flux
 *   - q-axis with torque-producing current
 *
 * Equations:
 *   d =  α·cos(θ) + β·sin(θ)
 *   q = -α·sin(θ) + β·cos(θ)
 *
 * @param[in]  input_ab  Pointer to αβ input structure
 * @param[out] output_dq Pointer to dq output structure
 * @param[in]  sinTheta  Sine of the electrical angle θ
 * @param[in]  cosTheta  Cosine of the electrical angle θ
 */
void FOC_ParkTransform(const AlphaBeta_t *input_ab, DQ_t *output_dq, float32 sinTheta, float32 cosTheta)
{
    /*
     * D =  Alpha * cos(th) + Beta * sin(th)
     * Q = -Alpha * sin(th) + Beta * cos(th)
     */
    output_dq->d = ( input_ab->alpha * cosTheta) + (input_ab->beta * sinTheta);
    output_dq->q = (-input_ab->alpha * sinTheta) + (input_ab->beta * cosTheta);
}

/**
 * @brief Performs the Inverse Park Transformation (dq → αβ).
 *
 * Converts rotating dq frame quantities back into stationary
 * αβ components using the electrical angle θ.
 *
 * This transformation is typically applied before SVPWM modulation.
 *
 * Equations:
 *   α = d·cos(θ) - q·sin(θ)
 *   β = d·sin(θ) + q·cos(θ)
 *
 * @param[in]  input_dq  Pointer to dq input structure
 * @param[out] output_ab Pointer to αβ output structure
 * @param[in]  sinTheta  Sine of the electrical angle θ
 * @param[in]  cosTheta  Cosine of the electrical angle θ
 */
void FOC_InvParkTransform(const DQ_t *input_dq, AlphaBeta_t *output_ab, float32 sinTheta, float32 cosTheta)
{
    /*
     * Alpha = D * cos(th) - Q * sin(th)
     * Beta  = D * sin(th) + Q * cos(th)
     */
    output_ab->alpha = (input_dq->d * cosTheta) - (input_dq->q * sinTheta);
    output_ab->beta  = (input_dq->d * sinTheta) + (input_dq->q * cosTheta);
}

/**
 * @brief Generates SVPWM duty cycles using zero-sequence injection.
 *
 * Implements Space Vector PWM (SVPWM) using the min/max
 * zero-sequence voltage injection method.
 *
 * Processing steps:
 * 1. Convert αβ voltages to three-phase quantities using Inverse Clarke
 * 2. Determine minimum and maximum phase voltages
 * 3. Compute zero-sequence offset:
 *      Voffset = -0.5 · (Vmax + Vmin)
 * 4. Apply offset and clamp to valid duty-cycle range
 *
 * This method maximizes DC bus voltage utilization while
 * keeping phase voltages within limits.
 *
 * @param[in]  V_ab        Pointer to αβ voltage reference
 * @param[out] DutyCycles Pointer to output duty cycles (a, b, c),
 *                         normalized to the range [0.0 – 1.0]
 */
void FOC_SVPWM(const AlphaBeta_t *V_ab, ThreePhase_t *DutyCycles)
{
    ThreePhase_t V_phase_raw;
    float32 V_max, V_min;
    float32 V_offset;

    /* 1. Reuse Inverse Clarke to calculate raw virtual phase voltages */
    FOC_InvClarkeTransform(V_ab, &V_phase_raw);

    /* 2. Determine Min and Max voltages to calculate Zero Sequence Offset */
    /* Find Max */
    V_max = Ifx__maxf(Ifx__maxf(V_phase_raw.a, V_phase_raw.b), V_phase_raw.c); //aurix assembly max instruction

    /* Find Min */
    V_min = Ifx__minf(Ifx__minf(V_phase_raw.a, V_phase_raw.b), V_phase_raw.c); //aurix assembly min instruction

    /* * 3. Calculate Zero Sequence Offset
     * Offset = -0.5 * (Min + Max)
     */
    V_offset = -FOC_ONE_HALF * (V_max + V_min);

    /* 4. Apply offset, Scale, and Add 50% Bias
     * Map [-1.0 ... 1.0] range to [0.0 ... 1.0] duty cycle
     */
    float32 V_a_mod = (V_phase_raw.a + V_offset);
    float32 V_b_mod = (V_phase_raw.b + V_offset);
    float32 V_c_mod = (V_phase_raw.c + V_offset);

    /* * NOTE: Multiply by 0.5 if inputs are range -1.0 to 1.0.
     * Add 0.5 to center the AC waveform at 50% duty cycle.
     */
    DutyCycles->a = Ifx__saturatef((V_a_mod * 0.5f) + 0.5f, FOC_DUTY_MIN, FOC_DUTY_MAX);
    DutyCycles->b = Ifx__saturatef((V_b_mod * 0.5f) + 0.5f, FOC_DUTY_MIN, FOC_DUTY_MAX);
    DutyCycles->c = Ifx__saturatef((V_c_mod * 0.5f) + 0.5f, FOC_DUTY_MIN, FOC_DUTY_MAX);
}
