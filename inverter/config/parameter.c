#include "parameter.h"
#include "app_config.h"
#include <string.h>

typedef struct
{
    uint16_t      address;
    void          *ptr;
    ParameterType type;    // Evaluates to byte size for payload matching
    AccessType    access;  // R, W, or RW capability
} Parameter;

/* * The Dictionary Table.
 * Maps specific 16-bit addresses to variables in your AppConfig and AppState.
 * Note: Avoid writing directly to app config at runtime if possible.
 * TODO: implement a shadow mechanism to avoid writing to control loop parameters at runtime
 */
static const Parameter param_table[] = {
    // =========================================================================
    // APPLICATION CONFIGURATION (Read / Write)
    // =========================================================================

    // --- ADC Config (0x1000) ---
    {0x1000, &app_config.adc.steps,                TYPE_UINT16,  ACCESS_RW},
    {0x1001, &app_config.adc.supply,               TYPE_FLOAT32, ACCESS_RW},

    // --- Current Config (0x1100) ---
    {0x1100, &app_config.current.offset_u_adcsteps,    TYPE_UINT16,  ACCESS_RW},
    {0x1101, &app_config.current.offset_v_adcsteps,    TYPE_UINT16,  ACCESS_RW},
    {0x1102, &app_config.current.current_sense_factor, TYPE_FLOAT32, ACCESS_RW},

    // --- PWM Config (0x1200) ---
    {0x1200, &app_config.pwm.frequency,            TYPE_UINT32,  ACCESS_RW},

    // --- FOC Config (0x1300) ---
    {0x1300, &app_config.foc.motor_polepairs,      TYPE_UINT8,   ACCESS_RW},
    {0x1301, &app_config.foc.speedRefHz,           TYPE_FLOAT32, ACCESS_RW},
    {0x1302, &app_config.foc.voltageRef,           TYPE_FLOAT32, ACCESS_RW},
    {0x1303, &app_config.foc.calibration_ticks,    TYPE_UINT64,  ACCESS_RW},
    {0x1304, &app_config.foc.resolver_offset,      TYPE_FLOAT32, ACCESS_RW},
    {0x1305, &app_config.foc.id_ref,               TYPE_FLOAT32, ACCESS_RW},
    {0x1306, &app_config.foc.iq_ref,               TYPE_FLOAT32, ACCESS_RW},

    // --- FOC PI ID Config (0x1400) ---
    {0x1400, &app_config.foc.pi_config_id.Kp,      TYPE_FLOAT32, ACCESS_RW},
    {0x1401, &app_config.foc.pi_config_id.Ki,      TYPE_FLOAT32, ACCESS_RW},
    {0x1402, &app_config.foc.pi_config_id.outMax,  TYPE_FLOAT32, ACCESS_RW},
    {0x1403, &app_config.foc.pi_config_id.outMin,  TYPE_FLOAT32, ACCESS_RW},

    // --- FOC PI IQ Config (0x1500) ---
    {0x1500, &app_config.foc.pi_config_iq.Kp,      TYPE_FLOAT32, ACCESS_RW},
    {0x1501, &app_config.foc.pi_config_iq.Ki,      TYPE_FLOAT32, ACCESS_RW},
    {0x1502, &app_config.foc.pi_config_iq.outMax,  TYPE_FLOAT32, ACCESS_RW},
    {0x1503, &app_config.foc.pi_config_iq.outMin,  TYPE_FLOAT32, ACCESS_RW},

    // --- Resolver Config (0x1600) ---
    {0x1600, &app_config.resolver.sin_max,         TYPE_UINT16,  ACCESS_RW},
    {0x1601, &app_config.resolver.sin_min,         TYPE_UINT16,  ACCESS_RW},
    {0x1602, &app_config.resolver.cos_max,         TYPE_UINT16,  ACCESS_RW},
    {0x1603, &app_config.resolver.cos_min,         TYPE_UINT16,  ACCESS_RW},
    {0x1604, &app_config.resolver.offset_sin,      TYPE_UINT16,  ACCESS_RW},
    {0x1605, &app_config.resolver.offset_cos,      TYPE_UINT16,  ACCESS_RW},
    {0x1606, &app_config.resolver.pole_pairs,      TYPE_UINT8,   ACCESS_RW},


    // =========================================================================
    // LIVE STATE VARIABLES (Read Only)
    // =========================================================================

    // --- FOC General State (0x2000) ---
    {0x2000, &app_state.foc.electricalAngle,       TYPE_FLOAT32, ACCESS_R},
    {0x2001, &app_state.foc.calibrated,            TYPE_UINT8,   ACCESS_R},

    // --- FOC Currents & Voltages (0x2100) ---
    // Note: Assuming struct fields are 'a', 'b', 'd', 'q'
    {0x2100, &app_state.foc.i_ab.alpha,                TYPE_FLOAT32, ACCESS_R},
    {0x2101, &app_state.foc.i_ab.beta,                TYPE_FLOAT32, ACCESS_R},
    {0x2102, &app_state.foc.v_ab.alpha,                TYPE_FLOAT32, ACCESS_R},
    {0x2103, &app_state.foc.v_ab.beta,                TYPE_FLOAT32, ACCESS_R},

    {0x2104, &app_state.foc.i_dq.d,                TYPE_FLOAT32, ACCESS_R},
    {0x2105, &app_state.foc.i_dq.q,                TYPE_FLOAT32, ACCESS_R},
    {0x2106, &app_state.foc.v_dq.d,                TYPE_FLOAT32, ACCESS_R},
    {0x2107, &app_state.foc.v_dq.q,                TYPE_FLOAT32, ACCESS_R},

    // --- Inverter Duty Cycles (0x2200) ---
    // Note: Assuming struct fields are 'u', 'v', 'w'
    {0x2200, &app_state.foc.duty_3ph.u,            TYPE_FLOAT32, ACCESS_R},
    {0x2201, &app_state.foc.duty_3ph.v,            TYPE_FLOAT32, ACCESS_R},
    {0x2202, &app_state.foc.duty_3ph.w,            TYPE_FLOAT32, ACCESS_R},

    // --- FOC PI Controller States (0x2300) ---
    {0x2300, &app_state.foc.pi_state_id.integral,  TYPE_FLOAT32, ACCESS_R},
    {0x2301, &app_state.foc.pi_state_iq.integral,  TYPE_FLOAT32, ACCESS_R},

    // --- Resolver Live State (0x2400) ---
    {0x2400, &app_state.resolver.prev_elec_angle,  TYPE_FLOAT32, ACCESS_R},
    {0x2401, &app_state.resolver.sector,           TYPE_INT8,    ACCESS_R},
};

#define PARAM_TABLE_SIZE (sizeof(param_table) / sizeof(param_table[0]))

bool readParameter (uint16_t address, void *out_data, uint8_t *out_len)
{
    for (int i = 0; i < PARAM_TABLE_SIZE; i++)
    {
        if (param_table[i].address == address)
        {
            // Verify Read Access
            if ((param_table[i].access & ACCESS_R) == 0)
            {
                return false; // Access Denied
            }

            *out_len = param_table[i].type; // Type doubles as size
            memcpy(out_data, param_table[i].ptr, *out_len);
            return true;
        }
    }
    return false;
}

bool writeParameter (uint16_t address, const void *in_data, uint8_t in_len)
{
    for (int i = 0; i < PARAM_TABLE_SIZE; i++)
    {
        if (param_table[i].address == address)
        {
            // Verify Write Access
            if ((param_table[i].access & ACCESS_W) == 0)
            {
                return false; // Access Denied
            }

            uint8_t expected_len = param_table[i].type; // Type doubles as size
            if (in_len != expected_len)
            {
                return false; // Safety check: incorrect payload size
            }

            memcpy(param_table[i].ptr, in_data, expected_len);
            return true;
        }
    }
    return false;
}
