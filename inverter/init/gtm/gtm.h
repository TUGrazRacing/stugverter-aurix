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

typedef enum
{
    GtmDriverDataFrameType_unknown = 0U,
    GtmDriverDataFrameType_adc,
    GtmDriverDataFrameType_diag0,
    GtmDriverDataFrameType_diag1
} GtmDriverDataFrameType;

typedef struct
{
    uint16 adc;
    uint16 diagnosticFrame0;
    uint16 diagnosticFrame1;
    uint16 lastRawFrame;
    GtmDriverDataFrameType lastFrameType;
    boolean adcValid;
    boolean diagnosticFrame0Valid;
    boolean diagnosticFrame1Valid;
} GtmDriverDataReadout;

void pwmInit(PwmConfig *pconfig);
void setDutyCycles(float32 dutyU, float32 dutyV, float32 dutyW);

void gtmDriverDataTimInit(void);
boolean gtmDriverDataTimUpdate(void);
const GtmPwmInputMeasurement *gtmDriverDataTimGetMeasurement(void);
const GtmDriverDataReadout *gtmDriverDataGetReadout(void);

void initTIM(void);
void measure_PWM(void);

#endif /* GTM_H_ */
