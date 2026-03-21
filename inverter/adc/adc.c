#include "adc.h"
#include "serialio.h"

#include <IfxCpu_Irq.h>
#include <IfxCpu.h>
#include <IfxSrc.h>
#include <IfxStm.h>
#include <IfxScuWdt.h>

#define G0_ALIAS   2u
#define G0_GROUP                 0u  /* master */
#define G1_GROUP                 1u
#define G2_GROUP                 2u
#define G3_GROUP                 3u

#define MASTER_GROUP             G0_GROUP
#define CHANNEL                  0u
#define QUEUE                    0u
#define INPUTCLASS               0u
#define RESREG0                  0u

#define EVADC_TRIGGER_SOURCE     11u
#define EVADC_TRIGGER_MODE       1u   /* rising edge */
#define EVADC_GATING_MODE        1u   /* always */

#define EVADC_QINR_RF            (1u << 5)
#define EVADC_QINR_EXTR          (1u << 7)

static void wait_us(uint32 us)
{
    IfxStm_waitTicks(&MODULE_STM0,
        IfxStm_getTicksFromMicroseconds(&MODULE_STM0, us));
}

static void initGroup(uint32 group)
{
    MODULE_EVADC.G[group].ANCFG.B.DIVA  = 0;
    MODULE_EVADC.G[group].ANCFG.B.DPCAL = 1;

    MODULE_EVADC.G[group].ICLASS[INPUTCLASS].U = 0;
    MODULE_EVADC.G[group].ICLASS[INPUTCLASS].B.STCS = 0xF;

    MODULE_EVADC.G[group].CHCTR[CHANNEL].U = 0;
    MODULE_EVADC.G[group].CHCTR[CHANNEL].B.ICLSEL = INPUTCLASS;
    MODULE_EVADC.G[group].CHCTR[CHANNEL].B.RESREG = RESREG0;
    MODULE_EVADC.G[group].CHCTR[CHANNEL].B.RESTGT = 0;
    MODULE_EVADC.G[group].CHCTR[CHANNEL].B.SYNC   = 1;

    // ONLY the master channel should drive the SYNC bus
    if (group == MASTER_GROUP) {
        MODULE_EVADC.G[group].CHCTR[CHANNEL].B.SYNC = 1;
    } else {
        MODULE_EVADC.G[group].CHCTR[CHANNEL].B.SYNC = 0;
    }
}

static void initSync(void)
{
    // G1 follows G0 (Master)
    MODULE_EVADC.G[G1_GROUP].SYNCTR.U = 0;
    MODULE_EVADC.G[G1_GROUP].SYNCTR.B.STSEL  = 0; // 0 = Group 0 is Master
    MODULE_EVADC.G[G1_GROUP].SYNCTR.B.EVALR1 = 1;
    MODULE_EVADC.G[G1_GROUP].SYNCTR.B.EVALR2 = 1;
    MODULE_EVADC.G[G1_GROUP].SYNCTR.B.EVALR3 = 1;

    // G0 is Master (No need to follow another group)
    MODULE_EVADC.G[G0_GROUP].SYNCTR.U = 0;
    MODULE_EVADC.G[G0_GROUP].SYNCTR.B.STSEL  = 0;
    MODULE_EVADC.G[G0_GROUP].SYNCTR.B.EVALR1 = 1;
    MODULE_EVADC.G[G0_GROUP].SYNCTR.B.EVALR2 = 1;
    MODULE_EVADC.G[G0_GROUP].SYNCTR.B.EVALR3 = 1;

    // G2 follows G0 (Master)
    MODULE_EVADC.G[G2_GROUP].SYNCTR.U = 0;
    MODULE_EVADC.G[G2_GROUP].SYNCTR.B.STSEL  = 0; // Changed from 2 to 0
    MODULE_EVADC.G[G2_GROUP].SYNCTR.B.EVALR1 = 1;
    MODULE_EVADC.G[G2_GROUP].SYNCTR.B.EVALR2 = 1;
    MODULE_EVADC.G[G2_GROUP].SYNCTR.B.EVALR3 = 1;

    // G3 follows G0 (Master)
    MODULE_EVADC.G[G3_GROUP].SYNCTR.U = 0;
    MODULE_EVADC.G[G3_GROUP].SYNCTR.B.STSEL  = 0; // Changed from 2 to 0
    MODULE_EVADC.G[G3_GROUP].SYNCTR.B.EVALR1 = 1;
    MODULE_EVADC.G[G3_GROUP].SYNCTR.B.EVALR2 = 1;
    MODULE_EVADC.G[G3_GROUP].SYNCTR.B.EVALR3 = 1;
}

static void initInterrupt(void)
{
    MODULE_EVADC.G[MASTER_GROUP].REFCLR.B.REV0 = 1;
    MODULE_EVADC.G[MASTER_GROUP].RCR[RESREG0].B.SRGEN = 1;
    MODULE_EVADC.G[MASTER_GROUP].REVNP0.B.REV0NP = 0;

    IfxSrc_init(&SRC_VADC_G0_SR0, ISR_PROVIDER_ADC, ISR_PRIORITY_ADC);
    IfxSrc_enable(&SRC_VADC_G0_SR0);
}

static void initQueue(void)
{
    Ifx_EVADC_G_Q_QCTRL qctrl;
    qctrl.U = 0; // Clear temp variable
    qctrl.B.XTWC   = 1;                    // Unlock write
    qctrl.B.XTSEL  = EVADC_TRIGGER_SOURCE; // 11
    qctrl.B.XTMODE = EVADC_TRIGGER_MODE;   // Rising edge
    MODULE_EVADC.G[MASTER_GROUP].Q[QUEUE].QCTRL.U = qctrl.U; // Write everything at once in a single instruction!

    /* always enabled queue, but conversion request waits for external trigger */
    MODULE_EVADC.G[MASTER_GROUP].Q[QUEUE].QMR.B.ENGT = EVADC_GATING_MODE;
    MODULE_EVADC.G[MASTER_GROUP].Q[QUEUE].QMR.B.ENTR = 1;

    MODULE_EVADC.G[MASTER_GROUP].ARBPR.U = 0;
    MODULE_EVADC.G[MASTER_GROUP].ARBPR.B.ASEN0 = 1;

    MODULE_EVADC.G[MASTER_GROUP].Q[QUEUE].QINR.U =
        ((uint32)CHANNEL << 0) |
        EVADC_QINR_RF |
        EVADC_QINR_EXTR;
}

void adcInit(void)
{
    //global module config
    MODULE_EVADC.CLC.B.DISR = 0;
    while(MODULE_EVADC.CLC.B.DISS); //wait for module clock

    MODULE_EVADC.GLOBCFG.B.SUPLEV = 0; //automatic voltage


    MODULE_EVADC.G[G0_GROUP].ARBCFG.B.ANONC = 0;
    MODULE_EVADC.G[G1_GROUP].ARBCFG.B.ANONC = 0;
    MODULE_EVADC.G[G2_GROUP].ARBCFG.B.ANONC = 0;
    MODULE_EVADC.G[G3_GROUP].ARBCFG.B.ANONC = 0;

    initGroup(G0_GROUP);
    initGroup(G1_GROUP);
    initGroup(G2_GROUP);
    initGroup(G3_GROUP);


    MODULE_EVADC.G[G0_GROUP].ALIAS.U = 0;
    MODULE_EVADC.G[G0_GROUP].ALIAS.B.ALIAS0 = G0_ALIAS;

    initSync();

    //normal operation
    MODULE_EVADC.G[G0_GROUP].ARBCFG.B.ANONC = 3;
    MODULE_EVADC.G[G2_GROUP].ARBCFG.B.ANONC = 3;
    MODULE_EVADC.G[G3_GROUP].ARBCFG.B.ANONC = 3;
    MODULE_EVADC.G[G1_GROUP].ARBCFG.B.ANONC = 3;

    wait_us(5);

    //wait for startup calibration to finish
    MODULE_EVADC.GLOBCFG.B.SUCAL = 1;
    while ((MODULE_EVADC.G[G0_GROUP].ARBCFG.B.CAL != 0) ||
           (MODULE_EVADC.G[G1_GROUP].ARBCFG.B.CAL != 0) ||
           (MODULE_EVADC.G[G2_GROUP].ARBCFG.B.CAL != 0) ||
           (MODULE_EVADC.G[G3_GROUP].ARBCFG.B.CAL != 0))
    {
    }

    initQueue();
    initInterrupt();

//    MODULE_EVADC.G[MASTER_GROUP].Q[QUEUE].QMR.B.TREV = 1;
}

void readAdc(uint16 *g0, uint16 *g1, uint16 *g2, uint16 *g3)
{
    *g0 = (uint16)MODULE_EVADC.G[G0_GROUP].RES[RESREG0].B.RESULT;
    *g1 = (uint16)MODULE_EVADC.G[G1_GROUP].RES[RESREG0].B.RESULT;
    *g2 = (uint16)MODULE_EVADC.G[G2_GROUP].RES[RESREG0].B.RESULT;
    *g3 = (uint16)MODULE_EVADC.G[G3_GROUP].RES[RESREG0].B.RESULT;
}
