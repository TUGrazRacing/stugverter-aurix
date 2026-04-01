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
