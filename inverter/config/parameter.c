#include "parameter.h"
#include "app_config.h"
#include <string.h>

/* Interne Strukturdefinition, passend zur Tabelle */
typedef struct
{
    uint16_t      address;
    void          *ptr;
    ParameterType type;    // Evaluates to byte size
    AccessType    access;  // R, W, or RW capability
    const char    *label;  // ASCII Name für das Dictionary
} Parameter;

static const Parameter param_table[] = {
    // --- ADC Config (0x1000) ---
    {0x1000, &app_config.adc.steps,                TYPE_UINT16,  ACCESS_RW, "ADC_STEPS"},
    {0x1001, &app_config.adc.supply,               TYPE_FLOAT32, ACCESS_RW, "ADC_SUPPLY"},

    // --- Current Config (0x1100) ---
    {0x1100, &app_config.current.offset_u_adcsteps,    TYPE_UINT16,  ACCESS_RW, "CUR_OFFS_U"},
    {0x1101, &app_config.current.offset_v_adcsteps,    TYPE_UINT16,  ACCESS_RW, "CUR_OFFS_V"},
    {0x1102, &app_config.current.current_sense_factor, TYPE_FLOAT32, ACCESS_RW, "CUR_SENSE_FAC"},

    // --- PWM Config (0x1200) ---
    {0x1200, &app_config.pwm.frequency,            TYPE_UINT32,  ACCESS_RW, "PWM_FREQ"},

    // --- FOC Config (0x1300) ---
    {0x1300, &app_config.foc.motor_polepairs,      TYPE_UINT8,   ACCESS_RW, "MOTOR_POLES"},
    {0x1301, &app_config.foc.speedRefHz,           TYPE_FLOAT32, ACCESS_RW, "SPEED_REF"},
    {0x1302, &app_config.foc.voltageRef,           TYPE_FLOAT32, ACCESS_RW, "VOLT_REF"},
    {0x1303, &app_config.foc.calibration_ticks,    TYPE_UINT64,  ACCESS_RW, "CALIB_TICKS"},
    {0x1304, &app_config.foc.resolver_offset,      TYPE_FLOAT32, ACCESS_RW, "RES_OFFSET"},
    {0x1305, &app_config.foc.id_ref,               TYPE_FLOAT32, ACCESS_RW, "ID_REF"},
    {0x1306, &app_config.foc.iq_ref,               TYPE_FLOAT32, ACCESS_RW, "IQ_REF"},

    // --- FOC PI ID Config (0x1400) ---
    {0x1400, &app_config.foc.pi_config_id.Kp,      TYPE_FLOAT32, ACCESS_RW, "ID_PI_KP"},
    {0x1401, &app_config.foc.pi_config_id.Ki,      TYPE_FLOAT32, ACCESS_RW, "ID_PI_KI"},
    {0x1402, &app_config.foc.pi_config_id.outMax,  TYPE_FLOAT32, ACCESS_RW, "ID_PI_MAX"},
    {0x1403, &app_config.foc.pi_config_id.outMin,  TYPE_FLOAT32, ACCESS_RW, "ID_PI_MIN"},

    // --- FOC PI IQ Config (0x1500) ---
    {0x1500, &app_config.foc.pi_config_iq.Kp,      TYPE_FLOAT32, ACCESS_RW, "IQ_PI_KP"},
    {0x1501, &app_config.foc.pi_config_iq.Ki,      TYPE_FLOAT32, ACCESS_RW, "IQ_PI_KI"},
    {0x1502, &app_config.foc.pi_config_iq.outMax,  TYPE_FLOAT32, ACCESS_RW, "IQ_PI_MAX"},
    {0x1503, &app_config.foc.pi_config_iq.outMin,  TYPE_FLOAT32, ACCESS_RW, "IQ_PI_MIN"},

    // --- Resolver Config (0x1600) ---
    {0x1600, &app_config.resolver.sin_max,         TYPE_UINT16,  ACCESS_RW, "RES_SIN_MAX"},
    {0x1601, &app_config.resolver.sin_min,         TYPE_UINT16,  ACCESS_RW, "RES_SIN_MIN"},
    {0x1602, &app_config.resolver.cos_max,         TYPE_UINT16,  ACCESS_RW, "RES_COS_MAX"},
    {0x1603, &app_config.resolver.cos_min,         TYPE_UINT16,  ACCESS_RW, "RES_COS_MIN"},
    {0x1604, &app_config.resolver.offset_sin,      TYPE_UINT16,  ACCESS_RW, "RES_OFF_SIN"},
    {0x1605, &app_config.resolver.offset_cos,      TYPE_UINT16,  ACCESS_RW, "RES_OFF_COS"},
    {0x1606, &app_config.resolver.pole_pairs,      TYPE_UINT8,   ACCESS_RW, "RES_POLES"},

    // --- FOC State (0x2000) ---
    {0x2000, &app_state.foc.electricalAngle,       TYPE_FLOAT32, ACCESS_R,  "ELEC_ANGLE"},
    {0x2001, &app_state.foc.calibrated,            TYPE_UINT8,   ACCESS_R,  "IS_CALIB"},

    // --- FOC Currents/Voltages (0x2100) ---
    {0x2100, &app_state.foc.i_ab.alpha,            TYPE_FLOAT32, ACCESS_R,  "I_ALPHA"},
    {0x2101, &app_state.foc.i_ab.beta,             TYPE_FLOAT32, ACCESS_R,  "I_BETA"},
    {0x2102, &app_state.foc.v_ab.alpha,            TYPE_FLOAT32, ACCESS_R,  "V_ALPHA"},
    {0x2103, &app_state.foc.v_ab.beta,             TYPE_FLOAT32, ACCESS_R,  "V_BETA"},
    {0x2104, &app_state.foc.i_dq.d,                TYPE_FLOAT32, ACCESS_R,  "I_D"},
    {0x2105, &app_state.foc.i_dq.q,                TYPE_FLOAT32, ACCESS_R,  "I_Q"},
    {0x2106, &app_state.foc.v_dq.d,                TYPE_FLOAT32, ACCESS_R,  "V_D"},
    {0x2107, &app_state.foc.v_dq.q,                TYPE_FLOAT32, ACCESS_R,  "V_Q"},

    // --- Duty Cycles (0x2200) ---
    {0x2200, &app_state.foc.duty_3ph.u,            TYPE_FLOAT32, ACCESS_R,  "DUTY_U"},
    {0x2201, &app_state.foc.duty_3ph.v,            TYPE_FLOAT32, ACCESS_R,  "DUTY_V"},
    {0x2202, &app_state.foc.duty_3ph.w,            TYPE_FLOAT32, ACCESS_R,  "DUTY_W"},

    // --- PI Integrals (0x2300) ---
    {0x2300, &app_state.foc.pi_state_id.integral,  TYPE_FLOAT32, ACCESS_R,  "ID_INTGL"},
    {0x2301, &app_state.foc.pi_state_iq.integral,  TYPE_FLOAT32, ACCESS_R,  "IQ_INTGL"},

    // --- Resolver State (0x2400) ---
    {0x2400, &app_state.resolver.prev_elec_angle,  TYPE_FLOAT32, ACCESS_R,  "PREV_ANGLE"},
    {0x2401, &app_state.resolver.sector,           TYPE_INT8,    ACCESS_R,  "SECTOR"},
};

#define PARAM_TABLE_SIZE (sizeof(param_table) / sizeof(param_table[0]))

/* --- API Funktionen --- */

bool readParameter(uint16_t address, void *out_data, uint8_t *out_len) {
    for (uint16_t i = 0; i < PARAM_TABLE_SIZE; i++) {
        if (param_table[i].address == address) {
            if (!(param_table[i].access & ACCESS_R)) return false;
            *out_len = (uint8_t)param_table[i].type;
            memcpy(out_data, param_table[i].ptr, *out_len);
            return true;
        }
    }
    return false;
}

bool writeParameter(uint16_t address, const void *in_data, uint8_t in_len) {
    for (uint16_t i = 0; i < PARAM_TABLE_SIZE; i++) {
        if (param_table[i].address == address) {
            if (!(param_table[i].access & ACCESS_W)) return false;
            if (in_len != (uint8_t)param_table[i].type) return false;
            memcpy(param_table[i].ptr, in_data, in_len);
            return true;
        }
    }
    return false;
}

bool getParameterInfo(uint16_t index, uint16_t *address, uint8_t *type, uint8_t *access, const char **label) {
    if (index < PARAM_TABLE_SIZE) {
        *address = param_table[index].address;
        *type = (uint8_t)param_table[index].type;
        // Spezifikation: 0 = R, 1 = RW
        *access = (param_table[index].access == ACCESS_RW) ? 1 : 0;
        *label = param_table[index].label;
        return true;
    }
    return false;
}

bool getParameterLen(uint16_t address, uint8_t *out_len) {
    for (uint16_t i = 0; i < PARAM_TABLE_SIZE; i++) {
        if (param_table[i].address == address) {
            *out_len = (uint8_t)param_table[i].type;
            return true;
        }
    }
    return false;
}

uint16_t getParameterCount(void) {
    return (uint16_t)PARAM_TABLE_SIZE;
}
