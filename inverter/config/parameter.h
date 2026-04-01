#ifndef PARAMETER_H_
#define PARAMETER_H_

#include <stdint.h>
#include <stdbool.h>

/* Assuming ParameterType resolves to byte size in your existing code
 * (e.g. TYPE_UINT16 = 2, TYPE_FLOAT32 = 4)
 */
typedef uint8_t ParameterType;
#define TYPE_UINT8   1
#define TYPE_INT8    1
#define TYPE_UINT16  2
#define TYPE_UINT32  4
#define TYPE_FLOAT32 4
#define TYPE_UINT64  8

/* Access Types using bitflags */
typedef enum
{
    ACCESS_NONE = 0x00,
    ACCESS_R    = 0x01,
    ACCESS_W    = 0x02,
    ACCESS_RW   = 0x03  /* (ACCESS_R | ACCESS_W) */
} AccessType;

/* Function Prototypes */
bool readParameter(uint16_t address, void *out_data, uint8_t *out_len);
bool writeParameter(uint16_t address, const void *in_data, uint8_t in_len);

#endif /* PARAMETER_H_ */
