#ifndef UART_H_
#define UART_H_

#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include "IfxAsclin_Asc.h"
#include "Asclin/Std/IfxAsclin.h"

#define SERIALIO_STRLEN 1024

/* Buffer used by the PRINTF_STRING macro */
extern char uart_print_buf[SERIALIO_STRLEN];

#define PRINT_STRING( x )    fprintf(stderr, x)
#define PRINTF_STRING(...) { do {sprintf(uart_print_buf, __VA_ARGS__); PRINT_STRING(uart_print_buf);} while(0);}

#ifdef __cplusplus
extern "C" {
#endif

/* Initializes the ASCLIN module for UART communication */
void UART_Init(sint32 baudrate);

/* Non-blocking task to parse incoming protocol messages. Call this in your main loop. */
void UART_Process(void);

#ifdef __cplusplus
}
#endif

#endif /* UART_H_ */
