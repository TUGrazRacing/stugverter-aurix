#include "params.h"
#include "app_config.h"
#include <string.h>

typedef struct {
    uint16_t    address;
    void* ptr;
    ParamType_t type;
} ParamDef_t;

/* * The Dictionary Table.
 * Maps specific 16-bit addresses to variables in your AppConfig.
 */
static const ParamDef_t param_table[] = {
    // --- ADC Config (0x1000) ---
    {0x1000, &app_config.adc.steps,                PARAM_TYPE_UINT16},
    {0x1001, &app_config.adc.supply,               PARAM_TYPE_FLOAT32},

    // --- Current Config (0x1100) ---
    {0x1100, &app_config.current.offset_u_adcsteps,    PARAM_TYPE_UINT16},
    {0x1101, &app_config.current.offset_v_adcsteps,    PARAM_TYPE_UINT16},
    {0x1102, &app_config.current.current_sense_factor, PARAM_TYPE_FLOAT32},

    // --- PWM Config (0x1200) ---
    {0x1200, &app_config.pwm.frequency,            PARAM_TYPE_UINT32},

    // --- FOC Config (0x1300) ---
    {0x1300, &app_config.foc.motor_polepairs,      PARAM_TYPE_UINT8},
    {0x1301, &app_config.foc.speedRefHz,           PARAM_TYPE_FLOAT32},
    {0x1302, &app_config.foc.voltageRef,           PARAM_TYPE_FLOAT32},
    {0x1303, &app_config.foc.calibration_ticks,    PARAM_TYPE_UINT64},
    {0x1304, &app_config.foc.resolver_offset,      PARAM_TYPE_FLOAT32},
    {0x1305, &app_config.foc.id_ref,               PARAM_TYPE_FLOAT32},
    {0x1306, &app_config.foc.iq_ref,               PARAM_TYPE_FLOAT32},

    // --- FOC PI ID Config (0x1400) ---
    {0x1400, &app_config.foc.pi_config_id.Kp,      PARAM_TYPE_FLOAT32},
    {0x1401, &app_config.foc.pi_config_id.Ki,      PARAM_TYPE_FLOAT32},
    {0x1402, &app_config.foc.pi_config_id.outMax,  PARAM_TYPE_FLOAT32},
    {0x1403, &app_config.foc.pi_config_id.outMin,  PARAM_TYPE_FLOAT32},

    // --- FOC PI IQ Config (0x1500) ---
    {0x1500, &app_config.foc.pi_config_iq.Kp,      PARAM_TYPE_FLOAT32},
    {0x1501, &app_config.foc.pi_config_iq.Ki,      PARAM_TYPE_FLOAT32},
    {0x1502, &app_config.foc.pi_config_iq.outMax,  PARAM_TYPE_FLOAT32},
    {0x1503, &app_config.foc.pi_config_iq.outMin,  PARAM_TYPE_FLOAT32},

    // --- Resolver Config (0x1600) ---
    {0x1600, &app_config.resolver.sin_max,         PARAM_TYPE_UINT16},
    {0x1601, &app_config.resolver.sin_min,         PARAM_TYPE_UINT16},
    {0x1602, &app_config.resolver.cos_max,         PARAM_TYPE_UINT16},
    {0x1603, &app_config.resolver.cos_min,         PARAM_TYPE_UINT16},
    {0x1604, &app_config.resolver.offset_sin,      PARAM_TYPE_UINT16},
    {0x1605, &app_config.resolver.offset_cos,      PARAM_TYPE_UINT16},
    {0x1606, &app_config.resolver.pole_pairs,      PARAM_TYPE_UINT8}
};

#define PARAM_TABLE_SIZE (sizeof(param_table) / sizeof(param_table[0]))

static uint8_t GetTypeSize(ParamType_t type) {
    switch(type) {
        case PARAM_TYPE_UINT8:   return 1;
        case PARAM_TYPE_UINT16:  return 2;
        case PARAM_TYPE_UINT32:  return 4;
        case PARAM_TYPE_FLOAT32: return 4;
        case PARAM_TYPE_UINT64:  return 8; // Added to support uint64 calibration_ticks
        default:                 return 0;
    }
}

bool Param_Read(uint16_t address, void* out_data, uint8_t* out_len) {
    for (int i = 0; i < PARAM_TABLE_SIZE; i++) {
        if (param_table[i].address == address) {
            *out_len = GetTypeSize(param_table[i].type);
            memcpy(out_data, param_table[i].ptr, *out_len);
            return true;
        }
    }
    return false; // Address not found
}

bool Param_Write(uint16_t address, const void* in_data, uint8_t in_len) {
    for (int i = 0; i < PARAM_TABLE_SIZE; i++) {
        if (param_table[i].address == address) {
            uint8_t expected_len = GetTypeSize(param_table[i].type);
            if (in_len != expected_len) {
                return false; // Safety check: incorrect payload size
            }
            memcpy(param_table[i].ptr, in_data, expected_len);
            return true;
        }
    }
    return false; // Address not found
}
