#include <can.h>

McmcanType         g_mcmcan;
IfxPort_Pin_Config g_led1;
IfxPort_Pin_Config g_led2;

IFX_INTERRUPT(canIsrTxHandler, 1, ISR_PRIORITY_CAN_TX);

static uint8 g_canCounter = 0;

static uint32 pack4(uint8 b0, uint8 b1, uint8 b2, uint8 b3)
{
    return ((uint32)b3 << 24) |
           ((uint32)b2 << 16) |
           ((uint32)b1 <<  8) |
           ((uint32)b0);
}

void canIsrTxHandler(void)
{
    IfxCan_Node_clearInterruptFlag(g_mcmcan.canSrcNode.node,
                                   IfxCan_Interrupt_transmissionCompleted);

    IfxPort_setPinLow(g_led1.port, g_led1.pinIndex);
}

void enableCanTransceiver(void)
{
    IfxPort_setPinModeOutput(&MODULE_P20, 6, IfxPort_OutputMode_pushPull,
                             IfxPort_OutputIdx_general);
    IfxPort_setPinLow(&MODULE_P20, 6);
}

void initMcmcan(void)
{
    const IfxCan_Can_Pins canPins =
    {
        &CAN0_TXD,                       IfxPort_OutputMode_pushPull,
        &CAN0_RXD,                       IfxPort_InputMode_pullUp,
        IfxPort_PadDriver_cmosAutomotiveSpeed1
    };

    IfxCan_Can_initModuleConfig(&g_mcmcan.canConfig, &MODULE_CAN0);
    IfxCan_Can_initModule(&g_mcmcan.canModule, &g_mcmcan.canConfig);

    IfxCan_Can_initNodeConfig(&g_mcmcan.canNodeConfig, &g_mcmcan.canModule);

    g_mcmcan.canNodeConfig.busLoopbackEnabled = FALSE;
    g_mcmcan.canNodeConfig.nodeId             = IfxCan_NodeId_0;
    g_mcmcan.canNodeConfig.frame.type         = IfxCan_FrameType_transmit;
    g_mcmcan.canNodeConfig.pins               = &canPins;
    g_mcmcan.canNodeConfig.baudRate.baudrate  = 1000000;

    g_mcmcan.canNodeConfig.interruptConfig.transmissionCompletedEnabled = TRUE;
    g_mcmcan.canNodeConfig.interruptConfig.traco.priority              = ISR_PRIORITY_CAN_TX;
    g_mcmcan.canNodeConfig.interruptConfig.traco.interruptLine         = IfxCan_InterruptLine_0;
    g_mcmcan.canNodeConfig.interruptConfig.traco.typeOfService         = IfxSrc_Tos_cpu1;

    IfxCan_Can_initNode(&g_mcmcan.canSrcNode, &g_mcmcan.canNodeConfig);
}

void transmitCanMessage(float32 i_u, float32 i_v, float32 theta)
{
    sint16 iu100 = (sint16)roundf(i_u * 100.0f);
    sint16 iv100 = (sint16)roundf(i_v * 100.0f);
    sint16 th100 = (sint16)roundf(theta * 100.0f);

    IfxCan_Can_initMessage(&g_mcmcan.txMsg);

    g_mcmcan.txData[0] = pack4(
        (uint8)(iu100 & 0xFF),
        (uint8)((iu100 >> 8) & 0xFF),
        (uint8)(iv100 & 0xFF),
        (uint8)((iv100 >> 8) & 0xFF)
    );

    g_mcmcan.txData[1] = pack4(
        (uint8)(th100 & 0xFF),
        (uint8)((th100 >> 8) & 0xFF),
        g_canCounter++,
        0u
    );

    g_mcmcan.txMsg.messageId      = CAN_MESSAGE_ID;
    g_mcmcan.txMsg.dataLengthCode = IfxCan_DataLengthCode_8;

    while (IfxCan_Status_notSentBusy ==
           IfxCan_Can_sendMessage(&g_mcmcan.canSrcNode,
                                  &g_mcmcan.txMsg,
                                  &g_mcmcan.txData[0]))
    {
    }
}
