#ifndef PARAMETER_H
#define PARAMETER_H

#include <stdbool.h>       // Adds support for 'bool', 'true', and 'false'
#include <stdint.h>        // Adds support for 'uint16_t', 'uint32_t', etc.
#include "Cpu/Std/Ifx_Types.h"

/* Die Enum-Werte entsprechen der Byte-Größe für die Payload-Verarbeitung */
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

/* API Prototypen */
bool readParameter(uint16_t address, void *out_data, uint8_t *out_len);
bool writeParameter(uint16_t address, const void *in_data, uint8_t in_len);
bool getParameterLen(uint16_t address, uint8_t *out_len);
uint16_t getParameterCount(void);

/* Wichtig für Dictionary: Muss den char** label enthalten */
bool getParameterInfo(uint16_t index, uint16_t *address, uint8_t *type, uint8_t *access, const char **label);

#endif
