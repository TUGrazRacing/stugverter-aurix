#ifndef APP_CONFIG_H_
#define APP_CONFIG_H_

#include <types_config.h>
#include <types_state.h>

typedef struct
{
    FocConfig      foc;
    ResolverConfig resolver;
    AdcConfig      adc;
    PwmConfig      pwm;
    CurrentConfig  current;
} AppConfig;

typedef struct
{
    FocState       foc;
    ResolverState  resolver;
} AppState;

extern AppConfig app_config;
extern AppState  app_state;

void initConfig(void);

#endif /* APP_CONFIG_H_ */
