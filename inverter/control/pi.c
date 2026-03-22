#include "pi.h"

void PI_Init(PiConfig *config, PiState *state, float32 kp, float32 ki, float32 min, float32 max)
{
    if ((config == NULL_PTR) || (state == NULL_PTR))
    {
        return;
    }

    config->Kp     = kp;
    config->Ki     = ki;
    config->outMin = min;
    config->outMax = max;

    state->integral = 0.0f;
}

float32 PI_Run(const PiConfig *config, PiState *state, float32 setpoint, float32 current)
{
    float32 error;
    float32 output;

    if ((config == NULL_PTR) || (state == NULL_PTR))
    {
        return 0.0f;
    }

    error = setpoint - current;

    state->integral += config->Ki * error;
    output = (config->Kp * error) + state->integral;

    if (output > config->outMax)
    {
        output = config->outMax;
        state->integral -= config->Ki * error;
    }
    else if (output < config->outMin)
    {
        output = config->outMin;
        state->integral -= config->Ki * error;
    }

    return output;
}
