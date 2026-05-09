#ifndef GTM_TIM_H_
#define GTM_TIM_H_

#include "Ifx_Types.h"

#define GTM_DRIVER_DATA_CHANNEL_COUNT (6U)

typedef enum
{
    GtmDriverDataChannel_uLow = 0U,
    GtmDriverDataChannel_vLow,
    GtmDriverDataChannel_wLow,
    GtmDriverDataChannel_uHigh,
    GtmDriverDataChannel_vHigh,
    GtmDriverDataChannel_wHigh,
    GtmDriverDataChannel_count
} GtmDriverDataChannelId;

typedef enum
{
    GtmDriverDataFrameType_unknown = 0U,
    GtmDriverDataFrameType_adc,
    GtmDriverDataFrameType_diag0,
    GtmDriverDataFrameType_diag1
} GtmDriverDataFrameType;

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

typedef struct
{
    GtmDriverDataChannelId channelId;
    boolean enabled;
    GtmPwmInputMeasurement pwm;
    GtmDriverDataReadout readout;
    uint32 frameCount;
    uint32 invalidFrameCount;
    uint32 dataLostCount;
    uint32 overflowCount;
    uint32 glitchCount;
} GtmDriverDataChannelState;

void gtmDriverDataTimInit(void);
boolean gtmDriverDataTimProcess(void);
boolean gtmDriverDataTimUpdate(void);
boolean gtmDriverDataTimCopyChannel(GtmDriverDataChannelId channelId, GtmDriverDataChannelState *channel);
const GtmDriverDataChannelState *gtmDriverDataTimGetChannel(GtmDriverDataChannelId channelId);
const GtmPwmInputMeasurement *gtmDriverDataTimGetMeasurement(void);
const GtmDriverDataReadout *gtmDriverDataGetReadout(void);
uint32 gtmDriverDataTimGetSequence(void);

void initTIM(void);
void measure_PWM(void);

#endif /* GTM_TIM_H_ */
