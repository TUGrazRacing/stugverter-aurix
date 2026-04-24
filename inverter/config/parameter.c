#include "parameter.h"
#include "app_config.h"
#include <string.h>

/* Internal struct reflecting the table layout */
typedef struct
{
    uint16_t      address;
    const void    *read_ptr;
    void          *write_ptr;
    ParameterType type;
    AccessType    access;
    uint8_t       group;
    const char    *label;
    uint8_t       unit[PARAM_UNIT_LEN];
} Parameter;

static const Parameter param_table[] = {
    // --- ADC Config (0x1000) ---
    {0x1000, &app_config.adc.steps,                &app_config_shadow.adc.steps,                TYPE_UINT16,  ACCESS_WRITE_AFTER_RESTART, 0x01, "ADC_STEPS",     ""},
    {0x1001, &app_config.adc.supply,               &app_config_shadow.adc.supply,               TYPE_FLOAT32, ACCESS_WRITE_AFTER_RESTART, 0x01, "ADC_SUPPLY",    "mV"},

    // --- Current Config (0x1100) ---
    {0x1100, &app_config.current.offset_u_adcsteps,    &app_config_shadow.current.offset_u_adcsteps,    TYPE_UINT16,  ACCESS_WRITE_AFTER_RESTART, 0x02, "CUR_OFFS_U",    ""},
    {0x1101, &app_config.current.offset_v_adcsteps,    &app_config_shadow.current.offset_v_adcsteps,    TYPE_UINT16,  ACCESS_WRITE_AFTER_RESTART, 0x02, "CUR_OFFS_V",    ""},
    {0x1102, &app_config.current.current_sense_factor, &app_config_shadow.current.current_sense_factor, TYPE_FLOAT32, ACCESS_WRITE_AFTER_RESTART, 0x02, "CUR_SENSE_FAC", "A/"}, // e.g. A/V or A/LSB

    // --- PWM Config (0x1200) ---
    {0x1200, &app_config.pwm.frequency,            &app_config_shadow.pwm.frequency,            TYPE_UINT32,  ACCESS_WRITE_AFTER_RESTART, 0x03, "PWM_FREQ",      "Hz"},

    // --- FOC Config (0x1300) ---
    {0x1300, &app_config.foc.motor_polepairs,      &app_config_shadow.foc.motor_polepairs,      TYPE_UINT8,   ACCESS_WRITE_AFTER_RESTART, 0x04, "MOTOR_POLES",   ""},
    {0x1301, &app_setpoints.foc.speedSetpointRpm,  &app_setpoints.foc.speedSetpointRpm,         TYPE_FLOAT32, ACCESS_WRITE_LIVE,          0x04, "SPEED_SET",    "rpm"},
    {0x1302, &app_setpoints.foc.voltageRef,        &app_setpoints.foc.voltageRef,               TYPE_FLOAT32, ACCESS_WRITE_LIVE,          0x04, "VOLT_REF",      "V"},
    {0x1303, &app_config.foc.calibration_ticks,    &app_config_shadow.foc.calibration_ticks,    TYPE_UINT64,  ACCESS_WRITE_AFTER_RESTART, 0x04, "CALIB_TICKS",   ""},
    {0x1304, &app_config.foc.resolver_offset,      &app_config_shadow.foc.resolver_offset,      TYPE_FLOAT32, ACCESS_WRITE_AFTER_RESTART, 0x04, "RES_OFFSET",    "deg"},
    {0x1305, &app_setpoints.foc.id_ref,            &app_setpoints.foc.id_ref,                   TYPE_FLOAT32, ACCESS_WRITE_LIVE,          0x04, "ID_REF",        "A"},
    {0x1306, &app_setpoints.foc.iq_ref,            &app_setpoints.foc.iq_ref,                   TYPE_FLOAT32, ACCESS_WRITE_LIVE,          0x04, "IQ_REF",        "A"},
    {0x1307, &app_setpoints.foc.controlMode,       &app_setpoints.foc.controlMode,              TYPE_UINT8,   ACCESS_WRITE_LIVE,          0x04, "CTRL_MODE",     ""},

    // --- FOC PI ID Config (0x1400) ---
    {0x1400, &app_config.foc.pi_config_id.Kp,      &app_config_shadow.foc.pi_config_id.Kp,      TYPE_FLOAT32, ACCESS_WRITE_AFTER_RESTART, 0x05, "ID_PI_KP",      ""},
    {0x1401, &app_config.foc.pi_config_id.Ki,      &app_config_shadow.foc.pi_config_id.Ki,      TYPE_FLOAT32, ACCESS_WRITE_AFTER_RESTART, 0x05, "ID_PI_KI",      ""},
    {0x1402, &app_config.foc.pi_config_id.outMax,  &app_config_shadow.foc.pi_config_id.outMax,  TYPE_FLOAT32, ACCESS_WRITE_AFTER_RESTART, 0x05, "ID_PI_MAX",     "V"},
    {0x1403, &app_config.foc.pi_config_id.outMin,  &app_config_shadow.foc.pi_config_id.outMin,  TYPE_FLOAT32, ACCESS_WRITE_AFTER_RESTART, 0x05, "ID_PI_MIN",     "V"},

    // --- FOC PI IQ Config (0x1500) ---
    {0x1500, &app_config.foc.pi_config_iq.Kp,      &app_config_shadow.foc.pi_config_iq.Kp,      TYPE_FLOAT32, ACCESS_WRITE_AFTER_RESTART, 0x06, "IQ_PI_KP",      ""},
    {0x1501, &app_config.foc.pi_config_iq.Ki,      &app_config_shadow.foc.pi_config_iq.Ki,      TYPE_FLOAT32, ACCESS_WRITE_AFTER_RESTART, 0x06, "IQ_PI_KI",      ""},
    {0x1502, &app_config.foc.pi_config_iq.outMax,  &app_config_shadow.foc.pi_config_iq.outMax,  TYPE_FLOAT32, ACCESS_WRITE_AFTER_RESTART, 0x06, "IQ_PI_MAX",     "V"},
    {0x1503, &app_config.foc.pi_config_iq.outMin,  &app_config_shadow.foc.pi_config_iq.outMin,  TYPE_FLOAT32, ACCESS_WRITE_AFTER_RESTART, 0x06, "IQ_PI_MIN",     "V"},

    // --- Speed PI Config (0x1700) ---
    {0x1700, &app_config.foc.pi_config_speed.Kp,      &app_config_shadow.foc.pi_config_speed.Kp,      TYPE_FLOAT32, ACCESS_WRITE_AFTER_RESTART, 0x07, "SPD_PI_KP",     ""},
    {0x1701, &app_config.foc.pi_config_speed.Ki,      &app_config_shadow.foc.pi_config_speed.Ki,      TYPE_FLOAT32, ACCESS_WRITE_AFTER_RESTART, 0x07, "SPD_PI_KI",     ""},
    {0x1702, &app_config.foc.pi_config_speed.outMax,  &app_config_shadow.foc.pi_config_speed.outMax,  TYPE_FLOAT32, ACCESS_WRITE_AFTER_RESTART, 0x07, "SPD_PI_MAX",    "A"},
    {0x1703, &app_config.foc.pi_config_speed.outMin,  &app_config_shadow.foc.pi_config_speed.outMin,  TYPE_FLOAT32, ACCESS_WRITE_AFTER_RESTART, 0x07, "SPD_PI_MIN",    "A"},

    // --- Resolver Config (0x1600) ---
    {0x1600, &app_config.resolver.sin_max,         &app_config_shadow.resolver.sin_max,         TYPE_UINT16,  ACCESS_WRITE_AFTER_RESTART, 0x08, "RES_SIN_MAX",   ""},
    {0x1601, &app_config.resolver.sin_min,         &app_config_shadow.resolver.sin_min,         TYPE_UINT16,  ACCESS_WRITE_AFTER_RESTART, 0x08, "RES_SIN_MIN",   ""},
    {0x1602, &app_config.resolver.cos_max,         &app_config_shadow.resolver.cos_max,         TYPE_UINT16,  ACCESS_WRITE_AFTER_RESTART, 0x08, "RES_COS_MAX",   ""},
    {0x1603, &app_config.resolver.cos_min,         &app_config_shadow.resolver.cos_min,         TYPE_UINT16,  ACCESS_WRITE_AFTER_RESTART, 0x08, "RES_COS_MIN",   ""},
    {0x1604, &app_config.resolver.offset_sin,      &app_config_shadow.resolver.offset_sin,      TYPE_UINT16,  ACCESS_WRITE_AFTER_RESTART, 0x08, "RES_OFF_SIN",   ""},
    {0x1605, &app_config.resolver.offset_cos,      &app_config_shadow.resolver.offset_cos,      TYPE_UINT16,  ACCESS_WRITE_AFTER_RESTART, 0x08, "RES_OFF_COS",   ""},
    {0x1606, &app_config.resolver.pole_pairs,      &app_config_shadow.resolver.pole_pairs,      TYPE_UINT8,   ACCESS_WRITE_AFTER_RESTART, 0x08, "RES_POLES",     ""},

    // --- FOC State (0x2000) ---
    {0x2000, &app_state.foc.electricalAngle,       NULL,                                        TYPE_FLOAT32, ACCESS_READ_ONLY,          0x20, "ELEC_ANGLE",    "deg"},
    {0x2001, &app_state.foc.calibrated,            NULL,                                        TYPE_UINT8,   ACCESS_READ_ONLY,          0x20, "IS_CALIB",      ""},

    // --- FOC Currents/Voltages (0x2100) ---
    {0x2100, &app_state.foc.i_ab.alpha,            NULL,                                        TYPE_FLOAT32, ACCESS_READ_ONLY,          0x21, "I_ALPHA",       "A"},
    {0x2101, &app_state.foc.i_ab.beta,             NULL,                                        TYPE_FLOAT32, ACCESS_READ_ONLY,          0x21, "I_BETA",        "A"},
    {0x2102, &app_state.foc.v_ab.alpha,            NULL,                                        TYPE_FLOAT32, ACCESS_READ_ONLY,          0x21, "V_ALPHA",       "V"},
    {0x2103, &app_state.foc.v_ab.beta,             NULL,                                        TYPE_FLOAT32, ACCESS_READ_ONLY,          0x21, "V_BETA",        "V"},
    {0x2104, &app_state.foc.i_dq.d,                NULL,                                        TYPE_FLOAT32, ACCESS_READ_ONLY,          0x21, "I_D",           "A"},
    {0x2105, &app_state.foc.i_dq.q,                NULL,                                        TYPE_FLOAT32, ACCESS_READ_ONLY,          0x21, "I_Q",           "A"},
    {0x2106, &app_state.foc.v_dq.d,                NULL,                                        TYPE_FLOAT32, ACCESS_READ_ONLY,          0x21, "V_D",           "V"},
    {0x2107, &app_state.foc.v_dq.q,                NULL,                                        TYPE_FLOAT32, ACCESS_READ_ONLY,          0x21, "V_Q",           "V"},

    // --- Duty Cycles (0x2200) ---
    {0x2200, &app_state.foc.duty_3ph.u,            NULL,                                        TYPE_FLOAT32, ACCESS_READ_ONLY,          0x22, "DUTY_U",        "%"},
    {0x2201, &app_state.foc.duty_3ph.v,            NULL,                                        TYPE_FLOAT32, ACCESS_READ_ONLY,          0x22, "DUTY_V",        "%"},
    {0x2202, &app_state.foc.duty_3ph.w,            NULL,                                        TYPE_FLOAT32, ACCESS_READ_ONLY,          0x22, "DUTY_W",        "%"},

    // --- PI Integrals (0x2300) ---
    {0x2300, &app_state.foc.pi_state_id.integral,  NULL,                                        TYPE_FLOAT32, ACCESS_READ_ONLY,          0x23, "ID_INTGL",      ""},
    {0x2301, &app_state.foc.pi_state_iq.integral,  NULL,                                        TYPE_FLOAT32, ACCESS_READ_ONLY,          0x23, "IQ_INTGL",      ""},
    {0x2302, &app_state.foc.pi_state_speed.integral, NULL,                                      TYPE_FLOAT32, ACCESS_READ_ONLY,          0x23, "SPD_INTGL",     ""},

    // --- Resolver State (0x2400) ---
    {0x2400, &app_state.resolver.prev_elec_angle,  NULL,                                        TYPE_FLOAT32, ACCESS_READ_ONLY,          0x24, "PREV_ANGLE",    "deg"},
    {0x2401, &app_state.resolver.sector,           NULL,                                        TYPE_INT8,    ACCESS_READ_ONLY,          0x24, "SECTOR",        ""},
    {0x2402, &app_state.foc.speed_mech_rpm,        NULL,                                        TYPE_FLOAT32, ACCESS_READ_ONLY,          0x24, "SPEED_RPM",     "rpm"},
    {0x2403, &app_state.foc.speed_iq_ref,          NULL,                                        TYPE_FLOAT32, ACCESS_READ_ONLY,          0x24, "SPD_IQ_REF",    "A"},
};

#define PARAM_TABLE_SIZE (sizeof(param_table) / sizeof(param_table[0]))

/* --- API Functions --- */

uint8_t getParameterSize(ParameterType type) {
    switch(type) {
        case TYPE_UINT8: case TYPE_INT8: return 1;
        case TYPE_UINT16: case TYPE_INT16: return 2;
        case TYPE_UINT32: case TYPE_INT32: case TYPE_FLOAT32: return 4;
        case TYPE_UINT64: return 8;
        default: return 0;
    }
}

bool readParameter(uint16_t address, void *out_data, uint8_t *out_len) {
    for (uint16_t i = 0; i < PARAM_TABLE_SIZE; i++) {
        if (param_table[i].address == address) {
            uint8_t len = getParameterSize(param_table[i].type);
            if ((out_data == NULL) || (out_len == NULL) || (param_table[i].read_ptr == NULL) || (len == 0U)) {
                return false;
            }
            *out_len = len;
            memcpy(out_data, param_table[i].read_ptr, len);
            return true;
        }
    }
    return false;
}

bool getParameterPointer(uint16_t address, const void **out_ptr, uint8_t *out_len) {
    for (uint16_t i = 0; i < PARAM_TABLE_SIZE; i++) {
        if (param_table[i].address == address) {
            uint8_t len = getParameterSize(param_table[i].type);
            if ((out_ptr == NULL) || (out_len == NULL) || (param_table[i].read_ptr == NULL) || (len == 0U)) {
                return false;
            }

            *out_ptr = param_table[i].read_ptr;
            *out_len = len;
            return true;
        }
    }

    return false;
}

bool writeParameter(uint16_t address, const void *in_data, uint8_t in_len) {
    for (uint16_t i = 0; i < PARAM_TABLE_SIZE; i++) {
        if (param_table[i].address == address) {
            if ((in_data == NULL) || (param_table[i].write_ptr == NULL)) {
                return false;
            }
            if ((param_table[i].access != ACCESS_WRITE_AFTER_RESTART) && (param_table[i].access != ACCESS_WRITE_LIVE)) {
                return false;
            }
            if (in_len != getParameterSize(param_table[i].type)) return false;
            memcpy(param_table[i].write_ptr, in_data, in_len);
            return true;
        }
    }
    return false;
}

bool getParameterLen(uint16_t address, uint8_t *out_len) {
    for (uint16_t i = 0; i < PARAM_TABLE_SIZE; i++) {
        if (param_table[i].address == address) {
            if (out_len == NULL) {
                return false;
            }
            *out_len = getParameterSize(param_table[i].type);
            return true;
        }
    }
    return false;
}

uint16_t getParameterCount(void) {
    return (uint16_t)PARAM_TABLE_SIZE;
}

bool getParameterClass(uint16_t address, ParameterClass *out_class) {
    for (uint16_t i = 0U; i < PARAM_TABLE_SIZE; i++) {
        if (param_table[i].address == address) {
            if (out_class == NULL) {
                return false;
            }

            switch (param_table[i].access) {
                case ACCESS_WRITE_AFTER_RESTART:
                    *out_class = PARAMETER_CLASS_CONFIG;
                    return true;

                case ACCESS_WRITE_LIVE:
                    *out_class = PARAMETER_CLASS_SETPOINT;
                    return true;

                case ACCESS_READ_ONLY:
                    *out_class = PARAMETER_CLASS_STATE;
                    return true;

                default:
                    return false;
            }
        }
    }

    return false;
}

/* Updated getParameterInfo with null-checks to allow safe searching */
bool getParameterInfo(uint16_t index, uint16_t *address, uint8_t *type, uint8_t *access, uint8_t *group, const char **label, uint8_t *unit) {
    if (index < PARAM_TABLE_SIZE) {
        if (address) *address = param_table[index].address;
        if (type) *type = (uint8_t)param_table[index].type;
        if (access) *access = (uint8_t)param_table[index].access;
        if (group) *group = param_table[index].group;
        if (label) *label = param_table[index].label;
        if (unit) {
            for (uint8_t i = 0U; i < PARAM_UNIT_LEN; i++) {
                uint8_t c = param_table[index].unit[i];
                unit[i] = (c == 0U) ? (uint8_t)' ' : c;
            }
        }
        return true;
    }
    return false;
}
