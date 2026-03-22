#include "resolver_math.h"
#include <math.h>

#define TWO_PI_F (6.28318530717958f)
#define PI_F     (3.14159265358979f)

static ResolverConfig *resolver_config = NULL_PTR;
static ResolverState  *resolver_state  = NULL_PTR;

void resolverInit(ResolverConfig *config, ResolverState *state)
{
    resolver_config = config;
    resolver_state  = state;
}

void resolverUpdateCalibration(ResolverConfig *config, uint16 sin_val, uint16 cos_val)
{
    if (config == NULL_PTR)
    {
        return;
    }

    if (sin_val > config->sin_max)
    {
        config->sin_max = sin_val;
    }
    if (cos_val > config->cos_max)
    {
        config->cos_max = cos_val;
    }

    if (sin_val < config->sin_min)
    {
        config->sin_min = sin_val;
    }
    if (cos_val < config->cos_min)
    {
        config->cos_min = cos_val;
    }

    config->offset_sin = (uint16)(((float32)config->sin_max + (float32)config->sin_min) * 0.5f);
    config->offset_cos = (uint16)(((float32)config->cos_max + (float32)config->cos_min) * 0.5f);
}

float32 resolverGetMechanicalAngle(uint16 sin_val, uint16 cos_val)
{
    float32 sin_centered;
    float32 cos_centered;
    float32 elec_angle;
    float32 delta;
    float32 mech_angle;

    sin_centered = (float32)sin_val - (float32)resolver_config->offset_sin;
    cos_centered = (float32)cos_val - (float32)resolver_config->offset_cos;
    elec_angle   = atan2f(sin_centered, cos_centered);

    if (elec_angle < 0.0f)
    {
        elec_angle += TWO_PI_F;
    }

    delta = elec_angle - resolver_state->prev_elec_angle;

    if (delta < -PI_F)
    {
      resolver_state->sector++;
    }
    else if (delta > PI_F)
    {
      resolver_state->sector--;
    }

    if (resolver_state->sector >= (sint8)resolver_config->pole_pairs)
    {
      resolver_state->sector = 0;
    }
    else if (resolver_state->sector < 0)
    {
      resolver_state->sector = (sint8)resolver_config->pole_pairs - 1;
    }

    resolver_state->prev_elec_angle = elec_angle;

    mech_angle = (elec_angle + ((float32)resolver_state->sector * TWO_PI_F)) / (float32)resolver_config->pole_pairs;
    return mech_angle;
}
