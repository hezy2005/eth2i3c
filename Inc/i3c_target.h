#ifndef I3C_TARGET_H
#define I3C_TARGET_H

#include "cmsis_os2.h"
#include "stm32h5xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

#define TARGET_MAX_COUNT 8U
#define TARGET_DEFAULT_STATIC_ADDR_7BIT 0x50U

typedef enum
{
    TARGET_PROTOCOL_I2C = 0,
    TARGET_PROTOCOL_I3C = 1
} TargetProtocol_t;

typedef struct
{
    uint8_t sa;
    uint8_t da;
    TargetProtocol_t protocol;
    uint64_t payload;
} TargetEntry_t;

typedef struct
{
    I3C_HandleTypeDef *hal;
    TargetEntry_t targets[TARGET_MAX_COUNT];
    TargetEntry_t *activeTarget;
    volatile uint32_t daaDone;
    volatile uint32_t daaError;
    uint8_t assignedCount;
    uint8_t nextDa;
    osMutexId_t busMutex;
} Controller_t;

bool ControllerInit(void);
Controller_t *ControllerGet(void);
Controller_t *ControllerGetByHal(I3C_HandleTypeDef *hal);
bool ControllerBusAcquire(uint32_t timeout);
void ControllerBusRelease(void);
void ControllerResetDaaState(void);

TargetEntry_t *TargetFindBySa(uint8_t sa);
TargetEntry_t *TargetFindByDa(uint8_t da);
TargetEntry_t *TargetCreateBySa(uint8_t sa);
TargetEntry_t *TargetCreateByDa(uint8_t da, uint64_t payload);
TargetEntry_t *TargetBind(uint8_t sa, uint8_t da);
bool TargetSetActiveBySa(uint8_t sa);
bool TargetSetActiveByDa(uint8_t da);
bool TargetSetProtocolBySa(uint8_t sa, TargetProtocol_t protocol);
bool TargetSetProtocolByDa(uint8_t da, TargetProtocol_t protocol);
void TargetHandleRstdaa(void);
const TargetEntry_t *TargetGetEntry(uint8_t index);
uint8_t TargetCount(void);

#endif
