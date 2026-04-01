/**********************************************************************************************************************
 * \file uart.h
 * \brief Minimal ASCLIN UART header for AURIX TC387
 *********************************************************************************************************************/
#ifndef UART_H_
#define UART_H_

#include "Ifx_Types.h"

/* Initializes the ASCLIN UART module */
void initSimpleUART(void);

/* Blocking loop that waits for a byte and immediately sends it back */
void echoUART(void);

#endif /* UART_H_ */
