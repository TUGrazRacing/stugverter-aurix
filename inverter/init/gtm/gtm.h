#ifndef GTM_H_
#define GTM_H_

#include <types_config.h>
#include "Ifx_Types.h"

#define PWM_FREQUENCY (20.0E3)

extern PwmConfig *pwm_config;

typedef struct
{
    float32 dutyPercent;
    float32 frequencyHz;
    float32 periodSeconds;
    uint32  periodTicks;
    uint32  pulseTicks;
    boolean dataCoherent;
    boolean newData;
    boolean dataLost;
    boolean overflow;
    boolean glitch;
    boolean initialized;
} GtmPwmInputMeasurement;

void pwmInit(PwmConfig *pconfig);
void setDutyCycles(float32 dutyU, float32 dutyV, float32 dutyW);

void gtmDriverDataTimInit(void);
boolean gtmDriverDataTimUpdate(void);
const GtmPwmInputMeasurement *gtmDriverDataTimGetMeasurement(void);

void initTIM(void);
void measure_PWM(void);

#endif /* GTM_H_ */
