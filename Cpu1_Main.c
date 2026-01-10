/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "adc.h"    /* Access to g_adcSharedData */
#include "stdio.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
/* Board Specific Settings */
#define ADC_VREF                (5.0f)      /* ADC Reference Voltage (Check if 3.3V or 5.0V) */
#define ADC_RESOLUTION          (4096.0f)   /* 12-bit ADC */

/* Sensor Specifications */
#define SENSOR_OFFSET_V         (1.9057f)     /* Voltage at 0 Amps */
#define SENSOR_SENSITIVITY      (0.0114f)   /* Sensitivity in V/A (11.4 mV/A) */

/*********************************************************************************************************************/
/*-------------------------------------------------Global Variables--------------------------------------------------*/
/*********************************************************************************************************************/
extern IfxCpu_syncEvent g_cpuSyncEvent;

/*********************************************************************************************************************/
/*--------------------------------------------Function Implementations-----------------------------------------------*/
/*********************************************************************************************************************/

/* Helper function to convert Raw ADC to Amps */
float32 convertAdcToAmps(float32 rawAdcValue)
{
    float32 voltage;
    float32 amps;

    /* 1. Convert Raw LSB to Voltage */
    /* Voltage = (Raw / 4096) * VREF */
    voltage = (rawAdcValue * ADC_VREF) / ADC_RESOLUTION;

    /* 2. Convert Voltage to Amps using Linear Equation */
    /* Voltage = (Current * Sensitivity) + Offset */
    /* Current = (Voltage - Offset) / Sensitivity */
    amps = (voltage - SENSOR_OFFSET_V) / SENSOR_SENSITIVITY;

    return amps;
}

void core1_main(void)
{
    float32 currentU, currentV, currentW;

    IfxCpu_enableInterrupts();
    
    /* Disable Watchdog for Core 1 (Simplification for debug) */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    
    /* Wait for CPU sync event */
    IfxCpu_emitEvent(&g_cpuSyncEvent);
    IfxCpu_waitEvent(&g_cpuSyncEvent, 1);
    
    while(1)
    {
        /* --- CONSUMER LOOP --- */
        /* Check if Core 0 has provided new ADC data */
        if(g_adcSharedData.newData == TRUE)
        {
            /* 1. Clear flag immediately */
            g_adcSharedData.newData = FALSE;

            /* 2. Convert Raw values to Amps */
            currentU = convertAdcToAmps(g_adcSharedData.u);
            currentV = convertAdcToAmps(g_adcSharedData.v);
            currentW = convertAdcToAmps(g_adcSharedData.w);

            /* 3. Print Results */
            /* Using %.2f for 2 decimal places of precision */
            printf("%.2f;%.2f;%.2f\n",
                   currentU,
                   currentV,
                   currentW);
        }
    }
}
