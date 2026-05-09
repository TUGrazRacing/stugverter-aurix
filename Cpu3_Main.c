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

/* ---------------- Buzzer / Megalovania Bench Toy ---------------- */

#define BUZZER_PIN              (&IfxPort_P33_0)
#define REST                    (0u)
#define NOTE_GATE_PERCENT       (88u)
#define SONG_REPEAT_GAP_MS      (1000u)

#define NOTE_AS3                (233u)
#define NOTE_B3                 (247u)
#define NOTE_C4                 (262u)
#define NOTE_D4                 (294u)
#define NOTE_F4                 (349u)
#define NOTE_G4                 (392u)
#define NOTE_GS4                (415u)
#define NOTE_A4                 (440u)
#define NOTE_AS4                (466u)
#define NOTE_B4                 (494u)
#define NOTE_C5                 (523u)
#define NOTE_D5                 (587u)
#define NOTE_F5                 (698u)
#define NOTE_G5                 (784u)
#define NOTE_GS5                (831u)
#define NOTE_A5                 (880u)
#define NOTE_AS5                (932u)
#define NOTE_B5                 (988u)
#define NOTE_C6                 (1047u)
#define NOTE_D6                 (1175u)

typedef struct
{
    uint16 frequencyHz;
    uint8 steps;
} BuzzerNote;

#define N(freq, steps)          { (freq), (steps) }

#define SONG_STEP_MS            (50u)  /* Fast and snappy for that 90s feel */

/* Nokia Tune Frequencies (Octaves 5 and 6) */
#define NOTE_CS5                (554u)
#define NOTE_D5                 (587u)
#define NOTE_E5                 (659u)
#define NOTE_FS5                (740u)
#define NOTE_GS5                (831u)
#define NOTE_A5                 (880u)
#define NOTE_B5                 (988u)
#define NOTE_CS6                (1109u)
#define NOTE_D6                 (1175u)
#define NOTE_E6                 (1319u)

#define N(freq, steps)          { (freq), (steps) }

static const BuzzerNote music[] =
{
    /* Phrase 1 */
    N(NOTE_E6, 2), N(NOTE_D6, 2), N(NOTE_FS5, 4), N(NOTE_GS5, 4),

    /* Phrase 2 */
    N(NOTE_CS6, 2), N(NOTE_B5, 2), N(NOTE_D5, 4), N(NOTE_E5, 4),

    /* Phrase 3 */
    N(NOTE_B5, 2), N(NOTE_A5, 2), N(NOTE_CS5, 4), N(NOTE_E5, 4),

    /* The final resting note */
    N(NOTE_A5, 8)
};

#undef N

#define SONG_LENGTH             ((uint32)(sizeof(music) / sizeof(music[0])))

/* ---------------- Delay Functions ---------------- */

static void buzzer_delayUs(uint32 us)
{
    uint32 ticks = (uint32)IfxStm_getTicksFromMicroseconds(&MODULE_STM0, us);
    IfxStm_waitTicks(&MODULE_STM0, ticks);
}

static void buzzer_delayMs(uint32 ms)
{
    uint32 ticks = (uint32)IfxStm_getTicksFromMilliseconds(&MODULE_STM0, ms);
    IfxStm_waitTicks(&MODULE_STM0, ticks);
}

/* ---------------- Buzzer Functions ---------------- */

static void buzzer_init(void)
{
    IfxPort_setPinModeOutput(BUZZER_PIN->port,
                             BUZZER_PIN->pinIndex,
                             IfxPort_OutputMode_pushPull,
                             IfxPort_OutputIdx_general);

    IfxPort_setPinLow(BUZZER_PIN->port, BUZZER_PIN->pinIndex);
}

static void buzzer_off(void)
{
    IfxPort_setPinLow(BUZZER_PIN->port, BUZZER_PIN->pinIndex);
}

static void buzzer_playTone(uint32 frequencyHz, uint32 durationMs)
{
    uint32 halfPeriodUs;
    uint32 cycles;
    uint32 i;

    if (frequencyHz == REST)
    {
        buzzer_off();
        buzzer_delayMs(durationMs);
        return;
    }

    halfPeriodUs = 500000u / frequencyHz;
    cycles = (durationMs * frequencyHz) / 1000u;

    for (i = 0u; i < cycles; i++)
    {
        IfxPort_setPinHigh(BUZZER_PIN->port, BUZZER_PIN->pinIndex);
        buzzer_delayUs(halfPeriodUs);

        IfxPort_setPinLow(BUZZER_PIN->port, BUZZER_PIN->pinIndex);
        buzzer_delayUs(halfPeriodUs);
    }

    buzzer_off();
}

static void buzzer_playNote(const BuzzerNote *note)
{
    uint32 durationMs = (uint32)note->steps * SONG_STEP_MS;
    uint32 toneMs = (durationMs * NOTE_GATE_PERCENT) / 100u;

    buzzer_playTone(note->frequencyHz, toneMs);

    if (toneMs < durationMs)
    {
        buzzer_delayMs(durationMs - toneMs);
    }
}

static void buzzer_playSong(void)
{
    uint32 i;

    for (i = 0u; i < SONG_LENGTH; i++)
    {
        buzzer_playNote(&music[i]);
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
        buzzer_delayMs(SONG_REPEAT_GAP_MS);
    }
}
