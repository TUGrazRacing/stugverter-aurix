#ifndef PARAM_DICT_H_
#define PARAM_DICT_H_

#include "Ifx_Types.h"
#include <stdbool.h>
#include <stdint.h>

/* Supported data types for parameters */
typedef enum {
    PARAM_TYPE_UINT8,
    PARAM_TYPE_UINT16,
    PARAM_TYPE_UINT32,
    PARAM_TYPE_UINT64,
    PARAM_TYPE_FLOAT32
} ParamType_t;

/* * Reads a parameter from the dictionary.
 * out_len is populated with the number of bytes read.
 */
bool Param_Read(uint16_t address, void* out_data, uint8_t* out_len);

/* * Writes a parameter to the dictionary.
 * in_len is checked against the expected type size to prevent memory corruption.
 */
bool Param_Write(uint16_t address, const void* in_data, uint8_t in_len);

#endif /* PARAM_DICT_H_ */
