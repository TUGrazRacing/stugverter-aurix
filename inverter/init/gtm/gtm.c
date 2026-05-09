#include "gtm.h"

#include "IfxGtm_Cmu.h"
#include "IfxGtm_reg.h"
#include "IfxGtm.h"

void gtmInit(void)
{
    gtmEnableClock0();
}

void gtmEnableClock0(void)
{
    if(!IfxGtm_isEnabled(&MODULE_GTM))
    {
        float32 frequency;

        IfxGtm_enable(&MODULE_GTM);

        frequency = IfxGtm_Cmu_getModuleFrequency(&MODULE_GTM);
        IfxGtm_Cmu_setGclkFrequency(&MODULE_GTM, frequency);
        IfxGtm_Cmu_setClkFrequency(&MODULE_GTM, IfxGtm_Cmu_Clk_0, frequency);
    }

    IfxGtm_Cmu_enableClocks(&MODULE_GTM, IFXGTM_CMU_CLKEN_CLK0);
}
