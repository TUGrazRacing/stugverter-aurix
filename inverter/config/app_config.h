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
    FocSetpoints foc;
} AppSetpoints;

typedef struct
{
    FocState       foc;
    ResolverState  resolver;
} AppState;

extern AppConfig app_config;
extern AppConfig app_config_shadow;
extern AppSetpoints app_setpoints;
extern AppState  app_state;

void initConfig(void);
void commitConfigShadow(void);
void updateFocResolverOffset(float32 resolver_offset);

#endif /* APP_CONFIG_H_ */
