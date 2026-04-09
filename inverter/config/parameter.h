#ifndef PARAMETER_H
#define PARAMETER_H

#include <stdbool.h>
#include <stdint.h>
#include "Cpu/Std/Ifx_Types.h"

/* Type enums matching payload byte sizes */
typedef enum {
    TYPE_UINT8   = 1,
    TYPE_INT8    = 1,
    TYPE_UINT16  = 2,
    TYPE_UINT32  = 4,
    TYPE_FLOAT32 = 4,
    TYPE_UINT64  = 8
} ParameterType;

typedef enum {
    ACCESS_R  = 0x01,
    ACCESS_W  = 0x02,
    ACCESS_RW = 0x03
} AccessType;

/* API Prototypes */
bool readParameter(uint16_t address, void *out_data, uint8_t *out_len);
bool writeParameter(uint16_t address, const void *in_data, uint8_t in_len);
bool getParameterLen(uint16_t address, uint8_t *out_len);
uint16_t getParameterCount(void);

/* Updated getParameterInfo with unit extraction */
bool getParameterInfo(uint16_t index, uint16_t *address, uint8_t *type, uint8_t *access, const char **label, uint8_t *unit);

#endif
