#include "i3c_bus.h"

#include "cmsis_os2.h"
#include "stm32h5xx_ll_i3c.h"

#include <stdio.h>

#define I3C_BUS_TIMEOUT_MS 3000U
#define I3C_BROADCAST_RSTDAA_VALUE 0x06U
#define I3C_TRANSFER_CONTROL_WORDS 32U

static void I3cBusDelayOneTick(void)
{
    if (osKernelGetState() == osKernelRunning)
    {
        (void)osDelay(1U);
    }
    else
    {
        HAL_Delay(1U);
    }
}

static HAL_StatusTypeDef I3cBusWaitCompletion(bool busyWait)
{
    Controller_t *controller = ControllerGet();
    uint32_t start = HAL_GetTick();

    while (true)
    {
        uint32_t error = HAL_I3C_GetError(controller->hal);
        HAL_I3C_StateTypeDef state = HAL_I3C_GetState(controller->hal);

        if (error != HAL_I3C_ERROR_NONE)
        {
            printf("I3C error: 0x%08lX\r\n", (unsigned long)error);
            return HAL_ERROR;
        }
        if (state == HAL_I3C_STATE_ERROR)
        {
            return HAL_ERROR;
        }
        if (state == HAL_I3C_STATE_READY)
        {
            return HAL_OK;
        }
        if ((HAL_GetTick() - start) >= I3C_BUS_TIMEOUT_MS)
        {
            return HAL_TIMEOUT;
        }
        if (!busyWait)
        {
            I3cBusDelayOneTick();
        }
    }
}

HAL_StatusTypeDef I3cBusGetActiveTransfer(uint8_t *target, uint32_t *stopOption, uint32_t *restartOption)
{
    TargetEntry_t *entry = ControllerGet()->activeTarget;

    if ((target == NULL) || (stopOption == NULL) || (restartOption == NULL) || (entry == NULL))
    {
        return HAL_ERROR;
    }

    if (entry->protocol == TARGET_PROTOCOL_I3C)
    {
        if (entry->da == 0U)
        {
            return HAL_ERROR;
        }
        *target = entry->da;
        *stopOption = I3C_PRIVATE_WITHOUT_ARB_STOP;
        *restartOption = I3C_PRIVATE_WITH_ARB_RESTART;
        return HAL_OK;
    }

    if (entry->sa == 0U)
    {
        return HAL_ERROR;
    }
    *target = entry->sa;
    *stopOption = I2C_PRIVATE_WITHOUT_ARB_STOP;
    *restartOption = I2C_PRIVATE_WITHOUT_ARB_RESTART;
    return HAL_OK;
}

HAL_StatusTypeDef I3cBusPrivateTransferLocked(uint8_t target, const uint8_t *txBuffer, uint32_t txLength,
                                             uint8_t *rxBuffer, uint32_t rxLength, uint32_t direction,
                                             uint32_t stopOption, bool busyWait)
{
    Controller_t *controller = ControllerGet();
    I3C_PrivateTypeDef privateDescription = {0};
    I3C_XferTypeDef transfer = {0};
    uint32_t controlBuffer[I3C_TRANSFER_CONTROL_WORDS] = {0};
    HAL_StatusTypeDef status;

    privateDescription.TargetAddr = target;
    privateDescription.Direction = direction;
    privateDescription.TxBuf.pBuffer = (uint8_t *)txBuffer;
    privateDescription.TxBuf.Size = txLength;
    privateDescription.RxBuf.pBuffer = rxBuffer;
    privateDescription.RxBuf.Size = rxLength;

    transfer.CtrlBuf.pBuffer = controlBuffer;
    transfer.CtrlBuf.Size = I3C_TRANSFER_CONTROL_WORDS;
    transfer.TxBuf.pBuffer = (uint8_t *)txBuffer;
    transfer.TxBuf.Size = txLength;
    transfer.RxBuf.pBuffer = rxBuffer;
    transfer.RxBuf.Size = rxLength;

    controller->hal->ErrorCode = HAL_I3C_ERROR_NONE;
    status = HAL_I3C_AddDescToFrame(controller->hal, NULL, &privateDescription, &transfer, 1U, stopOption);
    if (status != HAL_OK)
    {
        return status;
    }

    if (direction == HAL_I3C_DIRECTION_READ)
    {
        status = HAL_I3C_Ctrl_Receive_DMA(controller->hal, &transfer);
    }
    else
    {
        status = HAL_I3C_Ctrl_Transmit_DMA(controller->hal, &transfer);
    }
    if (status != HAL_OK)
    {
        return status;
    }
    return I3cBusWaitCompletion(busyWait);
}

HAL_StatusTypeDef I3cBusRandomReadLocked(uint8_t target, uint8_t reg, uint8_t *rxBuffer, uint32_t rxLength,
                                        uint32_t restartOption, bool busyWait)
{
    Controller_t *controller = ControllerGet();
    I3C_PrivateTypeDef privateDescription[2] = {0};
    I3C_XferTypeDef transfer = {0};
    uint32_t controlBuffer[I3C_TRANSFER_CONTROL_WORDS] = {0};
    uint8_t txBuffer[1] = {reg};
    HAL_StatusTypeDef status;

    privateDescription[0].TargetAddr = target;
    privateDescription[0].Direction = HAL_I3C_DIRECTION_WRITE;
    privateDescription[0].TxBuf.pBuffer = txBuffer;
    privateDescription[0].TxBuf.Size = sizeof(txBuffer);
    privateDescription[1].TargetAddr = target;
    privateDescription[1].Direction = HAL_I3C_DIRECTION_READ;
    privateDescription[1].RxBuf.pBuffer = rxBuffer;
    privateDescription[1].RxBuf.Size = rxLength;

    transfer.CtrlBuf.pBuffer = controlBuffer;
    transfer.CtrlBuf.Size = I3C_TRANSFER_CONTROL_WORDS;
    transfer.TxBuf.pBuffer = txBuffer;
    transfer.TxBuf.Size = sizeof(txBuffer);
    transfer.RxBuf.pBuffer = rxBuffer;
    transfer.RxBuf.Size = rxLength;

    controller->hal->ErrorCode = HAL_I3C_ERROR_NONE;
    status = HAL_I3C_AddDescToFrame(controller->hal, NULL, privateDescription, &transfer, 2U, restartOption);
    if (status != HAL_OK)
    {
        return status;
    }
    status = HAL_I3C_Ctrl_MultipleTransfer_DMA(controller->hal, &transfer);
    if (status != HAL_OK)
    {
        return status;
    }
    return I3cBusWaitCompletion(busyWait);
}

HAL_StatusTypeDef I3cBusPrivateTransfer(const uint8_t *txBuffer, uint32_t txLength,
                                       uint8_t *rxBuffer, uint32_t rxLength, uint32_t direction)
{
    HAL_StatusTypeDef status;
    uint8_t target;
    uint32_t stopOption;
    uint32_t restartOption;

    if (!ControllerBusAcquire(osWaitForever))
    {
        return HAL_ERROR;
    }

    status = I3cBusGetActiveTransfer(&target, &stopOption, &restartOption);
    if (status == HAL_OK)
    {
        status = I3cBusPrivateTransferLocked(target, txBuffer, txLength, rxBuffer, rxLength, direction,
                                             stopOption, false);
    }
    ControllerBusRelease();
    return status;
}

HAL_StatusTypeDef I3cBusRandomRead(uint8_t reg, uint8_t *rxBuffer, uint32_t rxLength)
{
    HAL_StatusTypeDef status;
    uint8_t target;
    uint32_t stopOption;
    uint32_t restartOption;

    if (!ControllerBusAcquire(osWaitForever))
    {
        return HAL_ERROR;
    }

    status = I3cBusGetActiveTransfer(&target, &stopOption, &restartOption);
    if (status == HAL_OK)
    {
        status = I3cBusRandomReadLocked(target, reg, rxBuffer, rxLength, restartOption, false);
    }
    ControllerBusRelease();
    return status;
}

HAL_StatusTypeDef I3cBusDirectCcc(uint8_t ccc, const uint8_t *txBuffer, uint32_t txLength,
                                 uint8_t *rxBuffer, uint32_t rxLength, uint32_t direction)
{
    Controller_t *controller = ControllerGet();
    TargetEntry_t *entry;
    I3C_CCCTypeDef cccDescription = {0};
    I3C_XferTypeDef transfer = {0};
    uint32_t controlBuffer[I3C_TRANSFER_CONTROL_WORDS] = {0};
    HAL_StatusTypeDef status;

    if (!ControllerBusAcquire(osWaitForever))
    {
        return HAL_ERROR;
    }

    entry = controller->activeTarget;
    if ((entry == NULL) || (entry->da == 0U))
    {
        ControllerBusRelease();
        return HAL_ERROR;
    }

    cccDescription.TargetAddr = entry->da;
    cccDescription.CCC = ccc;
    cccDescription.Direction = direction;
    cccDescription.CCCBuf.pBuffer = (direction == HAL_I3C_DIRECTION_READ) ? NULL : (uint8_t *)txBuffer;
    cccDescription.CCCBuf.Size = (direction == HAL_I3C_DIRECTION_READ) ? rxLength : txLength;

    transfer.CtrlBuf.pBuffer = controlBuffer;
    transfer.CtrlBuf.Size = I3C_TRANSFER_CONTROL_WORDS;
    transfer.TxBuf.pBuffer = (uint8_t *)txBuffer;
    transfer.TxBuf.Size = txLength;
    transfer.RxBuf.pBuffer = rxBuffer;
    transfer.RxBuf.Size = rxLength;

    controller->hal->ErrorCode = HAL_I3C_ERROR_NONE;
    status = HAL_I3C_AddDescToFrame(controller->hal, &cccDescription, NULL, &transfer, 1U,
                                    I3C_DIRECT_WITHOUT_DEFBYTE_STOP);
    if (status == HAL_OK)
    {
        if (direction == HAL_I3C_DIRECTION_READ)
        {
            status = HAL_I3C_Ctrl_ReceiveCCC_DMA(controller->hal, &transfer);
        }
        else
        {
            status = HAL_I3C_Ctrl_TransmitCCC_DMA(controller->hal, &transfer);
        }
    }
    if (status == HAL_OK)
    {
        status = I3cBusWaitCompletion(false);
    }

    ControllerBusRelease();
    return status;
}

HAL_StatusTypeDef I3cBusRstdaa(void)
{
    Controller_t *controller = ControllerGet();
    I3C_TypeDef *instance = controller->hal->Instance;
    uint32_t start;

    if (!ControllerBusAcquire(osWaitForever))
    {
        return HAL_ERROR;
    }

    start = HAL_GetTick();
    LL_I3C_ClearFlag_FC(instance);
    LL_I3C_ClearFlag_ERR(instance);
    LL_I3C_EnableArbitrationHeader(instance);
    LL_I3C_ControllerHandleCCC(instance, I3C_BROADCAST_RSTDAA_VALUE, 0U, LL_I3C_GENERATE_STOP);

    while ((__HAL_I3C_GET_FLAG(controller->hal, HAL_I3C_FLAG_FCF) == RESET) &&
           (__HAL_I3C_GET_FLAG(controller->hal, HAL_I3C_FLAG_ERRF) == RESET))
    {
        if ((HAL_GetTick() - start) >= I3C_BUS_TIMEOUT_MS)
        {
            ControllerBusRelease();
            return HAL_TIMEOUT;
        }
        I3cBusDelayOneTick();
    }

    if (__HAL_I3C_GET_FLAG(controller->hal, HAL_I3C_FLAG_ERRF) == SET)
    {
        LL_I3C_ClearFlag_ERR(instance);
        ControllerBusRelease();
        return HAL_ERROR;
    }

    LL_I3C_ClearFlag_FC(instance);
    ControllerResetDaaState();
    TargetHandleRstdaa();
    ControllerBusRelease();
    return HAL_OK;
}

HAL_StatusTypeDef I3cBusEntdaa(void)
{
    Controller_t *controller = ControllerGet();
    HAL_StatusTypeDef status;

    if (!ControllerBusAcquire(osWaitForever))
    {
        return HAL_ERROR;
    }

    ControllerResetDaaState();
    controller->hal->ErrorCode = HAL_I3C_ERROR_NONE;
    status = HAL_I3C_Ctrl_DynAddrAssign_IT(controller->hal, I3C_ONLY_ENTDAA);
    if (status == HAL_OK)
    {
        status = I3cBusWaitCompletion(false);
    }
    if ((status == HAL_OK) && (controller->daaError != 0U))
    {
        status = HAL_ERROR;
    }

    ControllerBusRelease();
    return status;
}

HAL_StatusTypeDef I3cBusNegotiateBySa(uint8_t sa, uint8_t *assignedDa)
{
    Controller_t *controller = ControllerGet();
    TargetEntry_t *entry;
    TargetEntry_t *newEntry = NULL;
    uint8_t previousDa[TARGET_MAX_COUNT];
    uint8_t newEntryCount = 0U;
    HAL_StatusTypeDef status;
    uint32_t index;

    if ((assignedDa == NULL) || !ControllerBusAcquire(osWaitForever))
    {
        return HAL_ERROR;
    }

    entry = TargetFindBySa(sa);
    if ((entry == NULL) || (entry->protocol != TARGET_PROTOCOL_I2C) || (entry->da != 0U))
    {
        ControllerBusRelease();
        return HAL_ERROR;
    }

    controller->hal->ErrorCode = HAL_I3C_ERROR_NONE;
    status = HAL_I3C_Ctrl_GenerateArbitration(controller->hal, 10U);
    if (status != HAL_OK)
    {
        ControllerBusRelease();
        return status;
    }

    for (index = 0U; index < TARGET_MAX_COUNT; ++index)
    {
        previousDa[index] = controller->targets[index].da;
    }

    ControllerResetDaaState();
    status = HAL_I3C_Ctrl_DynAddrAssign_IT(controller->hal, I3C_ONLY_ENTDAA);
    if (status == HAL_OK)
    {
        status = I3cBusWaitCompletion(false);
    }
    if ((status != HAL_OK) || (controller->daaError != 0U))
    {
        ControllerBusRelease();
        return (status == HAL_OK) ? HAL_ERROR : status;
    }

    if (entry->da == 0U)
    {
        for (index = 0U; index < TARGET_MAX_COUNT; ++index)
        {
            TargetEntry_t *candidate = &controller->targets[index];

            if ((previousDa[index] == 0U) && (candidate->da != 0U))
            {
                newEntry = candidate;
                ++newEntryCount;
            }
        }
        if ((newEntry == NULL) || (newEntryCount != 1U))
        {
            ControllerBusRelease();
            return HAL_ERROR;
        }
        if (newEntry != entry)
        {
            entry = TargetBind(sa, newEntry->da);
        }
    }

    if ((entry == NULL) || (entry->da == 0U) || !TargetSetProtocolBySa(sa, TARGET_PROTOCOL_I3C))
    {
        ControllerBusRelease();
        return HAL_ERROR;
    }

    *assignedDa = entry->da;
    ControllerBusRelease();
    return HAL_OK;
}
