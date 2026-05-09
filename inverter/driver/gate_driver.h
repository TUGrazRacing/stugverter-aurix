#ifndef GATE_DRIVER_H
#define GATE_DRIVER_H

#include "Ifx_Types.h"

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief Initializes the Gate Driver GPIOs.
 * Configures Reset and Enable pins as push-pull outputs.
 * Ensures all signals are LOW initially for safety.
 */
void gatedriverInit(void);

/**
 * @brief Polls the configured GTM TIM channel for the gate-driver DATA PWM signal.
 */
void gatedriverDataCapture(void);

/**
 * @brief Initializes the gate-driver DATA readout service.
 */
void gatedriverDataServiceInit(void);

/**
 * @brief Runs the gate-driver DATA readout service.
 */
void gatedriverDataServiceTask(void);

/**
 * @brief Runs the gate-driver DATA readout service forever.
 */
void gatedriverDataServiceRun(void);

/**
 * @brief Enables the Gate Driver outputs.
 * Sets the EN pins High.
 */
void gatedriverEnable(void);

/**
 * @brief Disables the Gate Driver outputs.
 * Sets the EN pins Low.
 */
void gatedriverDisable(void);

#endif /* GATE_DRIVER_H */
