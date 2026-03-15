#include "resolver_math.h"
#include <math.h>
#include <stddef.h> /* For NULL */
#include "logger.h"

/* Define single-precision constants (notice the 'f' suffix!) */
#define TWO_PI_F  (6.28318530717958f)

void resolverInit(Resolver_Config_t* config)
{
    if (config != NULL)
    {
        /* Initialize min/max limits for a 12-bit ADC */
        config->sin_max = 0;
        config->sin_min = 4095;
        config->cos_max = 0;
        config->cos_min = 4095;

        /* Initialize with safe mid-point defaults */
        config->offset_sin = 2050.0f;
        config->offset_cos = 2050.0f;

        /* Initialize tracking variables */
        config->prev_elec_angle = 0.0f;
        config->sector = 0; /* See the "Cold Boot" warning below */
        config->pole_pairs = 4;
    }
}

void resolverUpdateCalibration(Resolver_Config_t* config, uint16 sin_val, uint16 cos_val)
{
    if (config == NULL) return;

    /* 1. Track the Maximums */
    if (sin_val > config->sin_max) config->sin_max = sin_val;
    if (cos_val > config->cos_max) config->cos_max = cos_val;

    /* 2. Track the Minimums */
    if (sin_val < config->sin_min) config->sin_min = sin_val;
    if (cos_val < config->cos_min) config->cos_min = cos_val;
    /* 3. Calculate the new center point (offset) */
    /* Using * 0.5f saves CPU cycles compared to division (/ 2.0f) */
    config->offset_sin = (float32)(config->sin_max + config->sin_min) * 0.5f;
    config->offset_cos = (float32)(config->cos_max + config->cos_min) * 0.5f;
}

#define PI_F (3.14159265358979f)

float32 resolverGetMechanicalAngle(Resolver_Config_t* config, uint16 sin_val, uint16 cos_val)
{
    if (config == NULL) return 0.0f;

    /* 1. Calculate the electrical angle (same as before) */
    float32 sin_centered = (float32)sin_val - config->offset_sin;
    float32 cos_centered = (float32)cos_val - config->offset_cos;
    float32 elec_angle = atan2f(sin_centered, cos_centered);

    if (elec_angle < 0.0f) {
        elec_angle += TWO_PI_F;
    }

    /* 2. Wrap-around detection */
    /* Calculate the change since the last function call */
    float32 delta = elec_angle - config->prev_elec_angle;

    if (delta < -PI_F) {
        /* Angle dropped massively (e.g., 6.2 to 0.1) -> Wrapped forward */
        config->sector++;
    } else if (delta > PI_F) {
        /* Angle spiked massively (e.g., 0.1 to 6.2) -> Wrapped backward */
        config->sector--;
    }

    /* 3. Constrain the sector to [0, pole_pairs - 1] */
    if (config->sector >= config->pole_pairs) {
        config->sector = 0;
    } else if (config->sector < 0) {
        config->sector = config->pole_pairs - 1;
    }

    /* Save current angle for the next cycle's comparison */
    config->prev_elec_angle = elec_angle;

    /* 4. Calculate Absolute Mechanical Angle */
    /* Formula: Theta_mech = (Theta_elec + Sector * 2*PI) / Pole Pairs */
    float32 mech_angle = (elec_angle + (float32)config->sector * TWO_PI_F) / (float32)config->pole_pairs;

//    logPush(&(LogData_t){elec_angle, mech_angle, 0.0f});

    return mech_angle;
}
