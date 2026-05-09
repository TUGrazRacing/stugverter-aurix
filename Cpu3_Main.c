/**********************************************************************************************************************
 * \file Cpu3_Main.c
 *********************************************************************************************************************/

#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "IfxPort.h"
#include "IfxPort_Pinmap.h"
#include "IfxStm.h"

extern IfxCpu_syncEvent g_cpuSyncEvent;

/* ---------------- Buzzer / Song Settings ---------------- */

#define NOTE_GAP_MS     20u
#define REST            0u

/* Notes */
#define NOTE_C4         262u
#define NOTE_CS4        277u
#define NOTE_D4         294u
#define NOTE_DS4        311u
#define NOTE_E4         330u
#define NOTE_F4         349u
#define NOTE_FS4        370u
#define NOTE_G4         392u
#define NOTE_GS4        415u
#define NOTE_A4         440u
#define NOTE_AS4        466u
#define NOTE_B4         494u

#define NOTE_C5         523u
#define NOTE_CS5        554u
#define NOTE_D5         587u
#define NOTE_DS5        622u
#define NOTE_E5         659u
#define NOTE_F5         698u
#define NOTE_FS5        740u
#define NOTE_G5         784u
#define NOTE_GS5        831u
#define NOTE_A5         880u
#define NOTE_AS5        932u
#define NOTE_B5         988u

#define NOTE_C6         1047u

typedef struct
{
    uint16 frequencyHz;
    uint16 durationMs;
} BuzzerNote;

/*
 * Original fast 8-bit boss-style riff.
 * Not an exact transcription of Megalovania.
 */
static const BuzzerNote song[] =
{
    { NOTE_D4, 100 }, { NOTE_D4, 100 }, { NOTE_D5, 180 }, { NOTE_A4, 150 },
    { REST,    50 }, { NOTE_GS4,100 }, { NOTE_G4, 100 }, { NOTE_F4, 120 },
    { NOTE_D4, 100 }, { NOTE_F4, 100 }, { NOTE_G4, 100 },

    { NOTE_C4, 100 }, { NOTE_C4, 100 }, { NOTE_C5, 180 }, { NOTE_A4, 150 },
    { REST,    50 }, { NOTE_GS4,100 }, { NOTE_G4, 100 }, { NOTE_F4, 120 },
    { NOTE_D4, 100 }, { NOTE_F4, 100 }, { NOTE_G4, 100 },

    { NOTE_B4, 100 }, { NOTE_B4, 100 }, { NOTE_B5, 180 }, { NOTE_A5, 150 },
    { REST,    50 }, { NOTE_GS5,100 }, { NOTE_G5, 100 }, { NOTE_F5, 120 },
    { NOTE_D5, 100 }, { NOTE_F5, 100 }, { NOTE_G5, 100 },

    { NOTE_AS4,100 }, { NOTE_AS4,100 }, { NOTE_AS5,180 }, { NOTE_A5, 150 },
    { REST,    50 }, { NOTE_GS5,100 }, { NOTE_G5, 100 }, { NOTE_F5, 120 },
    { NOTE_D5, 100 }, { NOTE_F5, 100 }, { NOTE_G5, 100 },

    { NOTE_D5, 120 }, { NOTE_F5, 120 }, { NOTE_A5, 120 }, { NOTE_G5, 120 },
    { NOTE_F5, 120 }, { NOTE_D5, 120 }, { NOTE_C5, 120 }, { NOTE_D5, 240 },

    { REST,   120 }
};

#define SONG_LENGTH     ((uint32)(sizeof(song) / sizeof(song[0])))

/* ---------------- Delay Functions ---------------- */

static void delay_us(uint32 us)
{
    uint32 ticks = IfxStm_getTicksFromMicroseconds(&MODULE_STM0, us);
    IfxStm_waitTicks(&MODULE_STM0, ticks);
}

static void delay_ms(uint32 ms)
{
    uint32 ticks = IfxStm_getTicksFromMilliseconds(&MODULE_STM0, ms);
    IfxStm_waitTicks(&MODULE_STM0, ticks);
}

/* ---------------- Buzzer Functions ---------------- */

static void buzzer_init(void)
{
    IfxPort_setPinModeOutput(BUZZER->port,
                             BUZZER->pinIndex,
                             IfxPort_OutputMode_pushPull,
                             IfxPort_OutputIdx_general);

    IfxPort_setPinLow(BUZZER->port, BUZZER->pinIndex);
}

static void buzzer_off(void)
{
    IfxPort_setPinLow(BUZZER->port, BUZZER->pinIndex);
}

static void buzzer_playTone(uint32 frequencyHz, uint32 durationMs)
{
    uint32 halfPeriodUs;
    uint32 cycles;
    uint32 i;

    if (frequencyHz == REST)
    {
        buzzer_off();
        delay_ms(durationMs);
        return;
    }

    halfPeriodUs = 500000u / frequencyHz;
    cycles = (durationMs * frequencyHz) / 1000u;

    for (i = 0u; i < cycles; i++)
    {
        IfxPort_setPinHigh(BUZZER->port, BUZZER->pinIndex);
        delay_us(halfPeriodUs);

        IfxPort_setPinLow(BUZZER->port, BUZZER->pinIndex);
        delay_us(halfPeriodUs);
    }

    buzzer_off();
}

static void buzzer_playSong(void)
{
    uint32 i;

    for (i = 0u; i < SONG_LENGTH; i++)
    {
        buzzer_playTone(song[i].frequencyHz, song[i].durationMs);
        delay_ms(NOTE_GAP_MS);
    }
}

/* ---------------- Main ---------------- */

void core3_main(void)
{
    IfxCpu_enableInterrupts();

    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());

    IfxCpu_emitEvent(&g_cpuSyncEvent);
    IfxCpu_waitEvent(&g_cpuSyncEvent, 1);

    buzzer_init();

    while (1)
    {
        buzzer_playSong();
        delay_ms(1000u);
    }
}
