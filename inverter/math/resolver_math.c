#include "resolver_math.h"
#include <math.h>
#include <stddef.h> /* For NULL */

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

float32 resolverGetAngle(const Resolver_Config_t* config, uint16 sin_val, uint16 cos_val)
{
    if (config == NULL) return 0.0f;

    /* 1. Remove DC Offset to center the signals around 0 */
    float32 sin_centered = (float32)sin_val - config->offset_sin;
    float32 cos_centered = (float32)cos_val - config->offset_cos;

    /* 2. Calculate the angle in radians (-pi to +pi) */
    /* TriCore FPU will natively handle this efficiently */
    float32 angle = atan2f(sin_centered, cos_centered);

    /* 3. Normalize the result to the 0 to 2*PI range */
    if (angle < 0.0f)
    {
        angle += TWO_PI_F;
    }

    return angle;
}
