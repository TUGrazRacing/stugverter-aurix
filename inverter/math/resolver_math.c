#include "resolver_math.h"
#include <math.h>

/* Define single-precision constants (notice the 'f' suffix!) */
#define PI_F      (3.14159265358979f)
#define TWO_PI_F  (6.28318530717958f)

// State variables for calibration
static uint16 sin_max = 0;
static uint16 sin_min = 4095; // Assuming 12-bit ADC
static uint16 cos_max = 0;
static uint16 cos_min = 4095;

/* * Structure to hold calibration data.
 * Calculating these once during init saves CPU cycles in the ISR.
 */
typedef struct {
    float32 offset_sin;
    float32 offset_cos;
} Resolver_Config_t;

Resolver_Config_t config;

void Resolver_Math_UpdateCalibration(uint16 sin_val, uint16 cos_val)
{
    // 1. Track the Maximums
    if (sin_val > sin_max) sin_max = sin_val;
    if (cos_val > cos_max) cos_max = cos_val;

    // 2. Track the Minimums
    if (sin_val < sin_min) sin_min = sin_val;
    if (cos_val < cos_min) cos_min = cos_val;

    // 3. Calculate the new center point (offset)
    // We cast to float32 to feed directly into your angle calculation
    config->offset_sin = (float32)(sin_max + sin_min) / 2.0f;
    config->offset_cos = (float32)(cos_max + cos_min) / 2.0f;
}

float32 Resolver_Math_GetAngle(uint16 sin_val, uint16 cos_val)
{
    /* 1. Remove DC Offset to center the signals around 0 */
    float32 sin_centered = (float32)sin_val - config->offset_sin;
    float32 cos_centered = (float32)cos_val - config->offset_cos;

    /* 2. Calculate the angle in radians (-pi to +pi) */
    // Using the single-precision FPU accelerated atan2f
    float32 angle = atan2f(sin_centered, cos_centered);

    /* 3. Normalize the result to the 0 to 2*PI range */
    if (angle < 0.0f)
    {
        angle += TWO_PI_F;
    }

    return angle;
}
