#include "current_math.h"
#include <stdio.h>

static CurrentConfig *current_config = NULL_PTR;
static AdcConfig     *adc_config     = NULL_PTR;

void currentsGet(ThreePhaseCurrents *currents, uint16 adc_u, uint16 adc_v)
{
    float32 factor;

    if ((currents == NULL_PTR) || (current_config == NULL_PTR) || (adc_config == NULL_PTR))
    {
        return;
    }

    factor = adc_config->supply / (((float32)adc_config->steps) * current_config->current_sense_factor);
    currents->u = ((float32)adc_u - current_config->offset_u_adcsteps) * factor;
    currents->v = ((float32)adc_v - current_config->offset_v_adcsteps) * factor;
    currents->w = -(currents->u + currents->v);
}

void initCurrents(CurrentConfig *cconfig, AdcConfig *aconfig)
{
    current_config = cconfig;
    adc_config     = aconfig;
}
