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
    /* Latest valid DATA frames decoded from one gate driver. */
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

/**
 * @brief Initializes all configured TIM channels for gate-driver DATA capture.
 */
void gtmDriverDataTimInit(void);

/**
 * @brief Polls the configured TIM channels and decodes new DATA PWM frames.
 */
boolean gtmDriverDataTimProcess(void);

/**
 * @brief Backwards-compatible alias for gtmDriverDataTimProcess().
 */
boolean gtmDriverDataTimUpdate(void);

/**
 * @brief Copies one channel snapshot for use outside the TIM module.
 */
boolean gtmDriverDataTimCopyChannel(GtmDriverDataChannelId channelId, GtmDriverDataChannelState *channel);

/**
 * @brief Returns a direct pointer to one channel state for local debug use.
 */
const GtmDriverDataChannelState *gtmDriverDataTimGetChannel(GtmDriverDataChannelId channelId);

/**
 * @brief Returns the legacy single-channel PWM measurement.
 */
const GtmPwmInputMeasurement *gtmDriverDataTimGetMeasurement(void);

/**
 * @brief Returns the legacy single-channel DATA readout.
 */
const GtmDriverDataReadout *gtmDriverDataGetReadout(void);

/**
 * @brief Returns the lock-free snapshot sequence counter.
 */
uint32 gtmDriverDataTimGetSequence(void);

/**
 * @brief Legacy TIM initialization wrapper.
 */
void initTIM(void);

/**
 * @brief Legacy TIM polling wrapper.
 */
void measure_PWM(void);

#endif /* GTM_TIM_H_ */
