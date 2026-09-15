#include "i3c_target.h"

#include <string.h>

#define TARGET_PID_MASK 0x0000FFFFFFFFFFFFULL
#define TARGET_FIRST_DYNAMIC_ADDRESS 0x10U

extern I3C_HandleTypeDef hi3c1;

static Controller_t controller =
{
    .hal = &hi3c1,
    .nextDa = TARGET_FIRST_DYNAMIC_ADDRESS
};

static bool TargetIsEmpty(const TargetEntry_t *entry)
{
    return (entry->sa == 0U) && (entry->da == 0U);
}

static TargetEntry_t *TargetFindEmpty(void)
{
    uint32_t index;

    for (index = 0U; index < TARGET_MAX_COUNT; ++index)
    {
        if (TargetIsEmpty(&controller.targets[index]))
        {
            return &controller.targets[index];
        }
    }
    return NULL;
}

static TargetEntry_t *TargetFindUnassignedByPid(uint64_t payload)
{
    uint64_t pid = payload & TARGET_PID_MASK;
    uint32_t index;

    if (pid == 0U)
    {
        return NULL;
    }

    for (index = 0U; index < TARGET_MAX_COUNT; ++index)
    {
        TargetEntry_t *entry = &controller.targets[index];

        if ((entry->sa != 0U) && (entry->da == 0U) && ((entry->payload & TARGET_PID_MASK) == pid))
        {
            return entry;
        }
    }
    return NULL;
}

static void TargetSelectFirst(void)
{
    uint32_t index;

    controller.activeTarget = NULL;
    for (index = 0U; index < TARGET_MAX_COUNT; ++index)
    {
        if (!TargetIsEmpty(&controller.targets[index]))
        {
            controller.activeTarget = &controller.targets[index];
            return;
        }
    }
}

static bool TargetSetProtocol(TargetEntry_t *entry, TargetProtocol_t protocol)
{
    if ((entry == NULL) || ((protocol == TARGET_PROTOCOL_I3C) && (entry->da == 0U)) ||
        ((protocol == TARGET_PROTOCOL_I2C) && (entry->sa == 0U)))
    {
        return false;
    }
    if ((protocol != TARGET_PROTOCOL_I2C) && (protocol != TARGET_PROTOCOL_I3C))
    {
        return false;
    }

    entry->protocol = protocol;
    return true;
}

bool ControllerInit(void)
{
    static const osMutexAttr_t busMutexAttributes =
    {
        .name = "I3C1Bus"
    };

    memset(controller.targets, 0, sizeof(controller.targets));
    controller.activeTarget = NULL;
    ControllerResetDaaState();
    controller.busMutex = osMutexNew(&busMutexAttributes);
    if (controller.busMutex == NULL)
    {
        return false;
    }

    return TargetCreateBySa(TARGET_DEFAULT_STATIC_ADDR_7BIT) != NULL;
}

Controller_t *ControllerGet(void)
{
    return &controller;
}

Controller_t *ControllerGetByHal(I3C_HandleTypeDef *hal)
{
    return (controller.hal == hal) ? &controller : NULL;
}

bool ControllerBusAcquire(uint32_t timeout)
{
    return (controller.busMutex != NULL) && (osMutexAcquire(controller.busMutex, timeout) == osOK);
}

void ControllerBusRelease(void)
{
    if (controller.busMutex != NULL)
    {
        (void)osMutexRelease(controller.busMutex);
    }
}

void ControllerResetDaaState(void)
{
    controller.daaDone = 0U;
    controller.daaError = 0U;
    controller.assignedCount = 0U;
    controller.nextDa = TARGET_FIRST_DYNAMIC_ADDRESS;
}

TargetEntry_t *TargetFindBySa(uint8_t sa)
{
    uint32_t index;

    if ((sa == 0U) || (sa >= 0x7EU))
    {
        return NULL;
    }

    for (index = 0U; index < TARGET_MAX_COUNT; ++index)
    {
        if (controller.targets[index].sa == sa)
        {
            return &controller.targets[index];
        }
    }
    return NULL;
}

TargetEntry_t *TargetFindByDa(uint8_t da)
{
    uint32_t index;

    if ((da == 0U) || (da >= 0x7EU))
    {
        return NULL;
    }

    for (index = 0U; index < TARGET_MAX_COUNT; ++index)
    {
        if (controller.targets[index].da == da)
        {
            return &controller.targets[index];
        }
    }
    return NULL;
}

TargetEntry_t *TargetCreateBySa(uint8_t sa)
{
    TargetEntry_t *entry;

    if ((sa == 0U) || (sa >= 0x7EU) || (TargetFindBySa(sa) != NULL))
    {
        return NULL;
    }

    entry = TargetFindEmpty();
    if (entry == NULL)
    {
        return NULL;
    }

    memset(entry, 0, sizeof(*entry));
    entry->sa = sa;
    entry->protocol = TARGET_PROTOCOL_I2C;
    if (controller.activeTarget == NULL)
    {
        controller.activeTarget = entry;
    }
    return entry;
}

TargetEntry_t *TargetCreateByDa(uint8_t da, uint64_t payload)
{
    TargetEntry_t *entry;

    if ((da == 0U) || (da >= 0x7EU) || (TargetFindByDa(da) != NULL))
    {
        return NULL;
    }

    entry = TargetFindUnassignedByPid(payload);
    if (entry == NULL)
    {
        entry = TargetFindEmpty();
    }
    if (entry == NULL)
    {
        return NULL;
    }

    if (TargetIsEmpty(entry))
    {
        memset(entry, 0, sizeof(*entry));
    }
    entry->da = da;
    entry->protocol = TARGET_PROTOCOL_I3C;
    entry->payload = payload;
    if (controller.activeTarget == NULL)
    {
        controller.activeTarget = entry;
    }
    return entry;
}

TargetEntry_t *TargetBind(uint8_t sa, uint8_t da)
{
    TargetEntry_t *saEntry = TargetFindBySa(sa);
    TargetEntry_t *daEntry = TargetFindByDa(da);

    if ((saEntry == NULL) || (daEntry == NULL))
    {
        return NULL;
    }
    if (saEntry == daEntry)
    {
        saEntry->protocol = TARGET_PROTOCOL_I3C;
        return saEntry;
    }
    if ((saEntry->da != 0U) || (daEntry->sa != 0U))
    {
        return NULL;
    }

    daEntry->sa = sa;
    daEntry->protocol = TARGET_PROTOCOL_I3C;
    if (controller.activeTarget == saEntry)
    {
        controller.activeTarget = daEntry;
    }
    memset(saEntry, 0, sizeof(*saEntry));
    return daEntry;
}

bool TargetSetActiveBySa(uint8_t sa)
{
    TargetEntry_t *entry = TargetFindBySa(sa);

    if (entry == NULL)
    {
        return false;
    }
    controller.activeTarget = entry;
    return true;
}

bool TargetSetActiveByDa(uint8_t da)
{
    TargetEntry_t *entry = TargetFindByDa(da);

    if (entry == NULL)
    {
        return false;
    }
    controller.activeTarget = entry;
    return true;
}

bool TargetSetProtocolBySa(uint8_t sa, TargetProtocol_t protocol)
{
    return TargetSetProtocol(TargetFindBySa(sa), protocol);
}

bool TargetSetProtocolByDa(uint8_t da, TargetProtocol_t protocol)
{
    return TargetSetProtocol(TargetFindByDa(da), protocol);
}

void TargetHandleRstdaa(void)
{
    uint32_t index;

    for (index = 0U; index < TARGET_MAX_COUNT; ++index)
    {
        TargetEntry_t *entry = &controller.targets[index];

        if (entry->da == 0U)
        {
            continue;
        }
        if (entry->sa != 0U)
        {
            entry->da = 0U;
            entry->protocol = TARGET_PROTOCOL_I2C;
            continue;
        }
        if (controller.activeTarget == entry)
        {
            controller.activeTarget = NULL;
        }
        memset(entry, 0, sizeof(*entry));
    }

    if (controller.activeTarget == NULL)
    {
        TargetSelectFirst();
    }
}

const TargetEntry_t *TargetGetEntry(uint8_t index)
{
    return (index < TARGET_MAX_COUNT) ? &controller.targets[index] : NULL;
}

uint8_t TargetCount(void)
{
    uint8_t count = 0U;
    uint32_t index;

    for (index = 0U; index < TARGET_MAX_COUNT; ++index)
    {
        if (!TargetIsEmpty(&controller.targets[index]))
        {
            ++count;
        }
    }
    return count;
}
