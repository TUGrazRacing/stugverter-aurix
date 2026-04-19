#ifndef PARAMETER_H
#define PARAMETER_H

#include <stdbool.h>
#include <stdint.h>
#include "Cpu/Std/Ifx_Types.h"

#define PARAM_NAME_LEN         16U
#define PARAM_UNIT_LEN         5U
#define PARAM_DICT_RECORD_LEN  (2U + 1U + PARAM_NAME_LEN + PARAM_UNIT_LEN)

typedef enum {
    TYPE_UINT8   = 0,
    TYPE_INT8    = 1,
    TYPE_UINT16  = 2,
    TYPE_INT16   = 3,
    TYPE_UINT32  = 4,
    TYPE_INT32   = 5,
    TYPE_FLOAT32 = 6,
    TYPE_UINT64  = 7
} ParameterType;

typedef enum {
    ACCESS_READ_ONLY          = 0x00,
    ACCESS_WRITE_AFTER_RESTART = 0x01,
    ACCESS_WRITE_LIVE         = 0x02
} AccessType;

typedef enum {
    PARAMETER_CLASS_CONFIG = 0,
    PARAMETER_CLASS_SETPOINT = 1,
    PARAMETER_CLASS_STATE = 2
} ParameterClass;

/* API Prototypes */
bool readParameter(uint16_t address, void *out_data, uint8_t *out_len);
bool writeParameter(uint16_t address, const void *in_data, uint8_t in_len);
bool getParameterLen(uint16_t address, uint8_t *out_len);
uint16_t getParameterCount(void);
uint8_t getParameterSize(ParameterType type);
bool getParameterClass(uint16_t address, ParameterClass *out_class);

bool getParameterInfo(uint16_t index, uint16_t *address, uint8_t *type, uint8_t *access, const char **label, uint8_t *unit);

#endif
