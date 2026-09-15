#ifndef I3C_BUS_H
#define I3C_BUS_H

#include "i3c_target.h"

#include <stdint.h>

#define I3C_BUS_MAX_DATA_BYTES 2048U

HAL_StatusTypeDef I3cBusRstdaa(void);
HAL_StatusTypeDef I3cBusEntdaa(void);
HAL_StatusTypeDef I3cBusNegotiateBySa(uint8_t sa, uint8_t *assignedDa);
HAL_StatusTypeDef I3cBusDirectCcc(uint8_t ccc, const uint8_t *txBuffer, uint32_t txLength,
                                 uint8_t *rxBuffer, uint32_t rxLength, uint32_t direction);
HAL_StatusTypeDef I3cBusPrivateTransfer(const uint8_t *txBuffer, uint32_t txLength,
                                       uint8_t *rxBuffer, uint32_t rxLength, uint32_t direction);
HAL_StatusTypeDef I3cBusRandomRead(uint8_t reg, uint8_t *rxBuffer, uint32_t rxLength);
HAL_StatusTypeDef I3cBusGetActiveTransfer(uint8_t *target, uint32_t *stopOption, uint32_t *restartOption);
HAL_StatusTypeDef I3cBusPrivateTransferLocked(uint8_t target, const uint8_t *txBuffer, uint32_t txLength,
                                             uint8_t *rxBuffer, uint32_t rxLength, uint32_t direction,
                                             uint32_t stopOption, bool busyWait);
HAL_StatusTypeDef I3cBusRandomReadLocked(uint8_t target, uint8_t reg, uint8_t *rxBuffer, uint32_t rxLength,
                                        uint32_t restartOption, bool busyWait);

#endif
