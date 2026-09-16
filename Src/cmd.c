#include "cmd.h"

#include "i3c_bus.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CMD_MAX_ARGUMENTS (I3C_BUS_MAX_DATA_BYTES + 8U)
#define CMD_ERROR_INVALID 0x0001U
#define CMD_ERROR_MISSING_PARAMETER 0x0004U
#define CMD_ERROR_FAILURE 0x0008U
#define CMD_ERROR_TIMEOUT 0x0010U
#define CMD_ERROR_OUT_OF_RANGE 0x0040U

#define I3C_CCC_ENEC 0x80U
#define I3C_CCC_DISEC 0x81U
#define I3C_CCC_SETMWL 0x89U
#define I3C_CCC_SETMRL 0x8AU
#define I3C_CCC_GETMWL 0x8BU
#define I3C_CCC_GETMRL 0x8CU
#define I3C_CCC_GETPID 0x8DU
#define I3C_CCC_GETBCR 0x8EU
#define I3C_CCC_GETDCR 0x8FU
#define I3C_CCC_GETSTATUS 0x90U

typedef void (*CmdHandler_t)(void);

typedef struct
{
    const char *name;
    CmdHandler_t handler;
    const char *usage;
} CmdEntry_t;

static char responseBuffer[CMD_RESPONSE_BUFFER_LENGTH];
static size_t responseLength;
static char *arguments[CMD_MAX_ARGUMENTS];
static uint32_t argumentCount;
static uint8_t transferBuffer[I3C_BUS_MAX_DATA_BYTES + 1U];

static void CmdHandleHelp(void);
static void CmdHandleTwi(void);
static void CmdHandleRstdaa(void);
static void CmdHandleEntdaa(void);
static void CmdHandleGetpid(void);
static void CmdHandleGetbcr(void);
static void CmdHandleGetdcr(void);
static void CmdHandleGetmwl(void);
static void CmdHandleGetmrl(void);
static void CmdHandleGetstatus(void);
static void CmdHandleSetmwl(void);
static void CmdHandleSetmrl(void);
static void CmdHandleEnec(void);
static void CmdHandleDisec(void);
static void CmdHandleTargetList(void);
static void CmdHandleTargetCreateBySa(void);
static void CmdHandleTargetSetActiveBySa(void);
static void CmdHandleTargetSetActiveByDa(void);
static void CmdHandleTargetBind(void);
static void CmdHandleTargetNegotiateBySa(void);
static void CmdHandleTargetSetProtocolBySa(void);
static void CmdHandleTargetSetProtocolByDa(void);

static const CmdEntry_t commandTable[] =
{
    {"*HELP", CmdHandleHelp, "*HELP"},
    {"*TWI", CmdHandleTwi, "*TWI len [reg [data...]]"},
    {"*I3C_RSTDAA", CmdHandleRstdaa, "*I3C_RSTDAA"},
    {"*I3C_ENTDAA", CmdHandleEntdaa, "*I3C_ENTDAA"},
    {"*I3C_GETPID", CmdHandleGetpid, "*I3C_GETPID"},
    {"*I3C_GETBCR", CmdHandleGetbcr, "*I3C_GETBCR"},
    {"*I3C_GETDCR", CmdHandleGetdcr, "*I3C_GETDCR"},
    {"*I3C_GETMWL", CmdHandleGetmwl, "*I3C_GETMWL"},
    {"*I3C_GETMRL", CmdHandleGetmrl, "*I3C_GETMRL"},
    {"*I3C_GETSTATUS", CmdHandleGetstatus, "*I3C_GETSTATUS"},
    {"*I3C_SETMWL", CmdHandleSetmwl, "*I3C_SETMWL high low"},
    {"*I3C_SETMRL", CmdHandleSetmrl, "*I3C_SETMRL high low"},
    {"*I3C_ENEC", CmdHandleEnec, "*I3C_ENEC mask"},
    {"*I3C_DISEC", CmdHandleDisec, "*I3C_DISEC mask"},
    {"*I3C_TARGET_LIST", CmdHandleTargetList, "*I3C_TARGET_LIST"},
    {"*I3C_TARGET_CREATE_BY_SA", CmdHandleTargetCreateBySa, "*I3C_TARGET_CREATE_BY_SA sa"},
    {"*I3C_TARGET_SET_ACTIVE_BY_SA", CmdHandleTargetSetActiveBySa, "*I3C_TARGET_SET_ACTIVE_BY_SA sa"},
    {"*I3C_TARGET_SET_ACTIVE_BY_DA", CmdHandleTargetSetActiveByDa, "*I3C_TARGET_SET_ACTIVE_BY_DA da"},
    {"*I3C_TARGET_BIND", CmdHandleTargetBind, "*I3C_TARGET_BIND sa da"},
    {"*I3C_TARGET_NEGOTIATE_I3C_BY_SA", CmdHandleTargetNegotiateBySa,
     "*I3C_TARGET_NEGOTIATE_I3C_BY_SA sa"},
    {"*I3C_TARGET_SET_PROTOCOL_BY_SA", CmdHandleTargetSetProtocolBySa,
     "*I3C_TARGET_SET_PROTOCOL_BY_SA sa protocol"},
    {"*I3C_TARGET_SET_PROTOCOL_BY_DA", CmdHandleTargetSetProtocolByDa,
     "*I3C_TARGET_SET_PROTOCOL_BY_DA da protocol"}
};

static void CmdUppercase(char *text)
{
    while (*text != '\0')
    {
        if ((*text >= 'a') && (*text <= 'z'))
        {
            *text = (char)(*text - ('a' - 'A'));
        }
        ++text;
    }
}

static bool CmdAppend(const char *format, ...)
{
    va_list args;
    int written;
    size_t remaining;

    if (responseLength >= sizeof(responseBuffer))
    {
        return false;
    }

    remaining = sizeof(responseBuffer) - responseLength;
    va_start(args, format);
    written = vsnprintf(&responseBuffer[responseLength], remaining, format, args);
    va_end(args);
    if ((written < 0) || ((size_t)written >= remaining))
    {
        return false;
    }

    responseLength += (size_t)written;
    return true;
}

static void CmdSetError(uint16_t code, const char *message)
{
    responseLength = 0U;
    responseBuffer[0] = '\0';
    if ((message == NULL) || (message[0] == '\0'))
    {
        (void)CmdAppend("$%04x;<>", code);
    }
    else
    {
        (void)CmdAppend("$%04x;<> %s", code, message);
    }
}

static void CmdSetHalError(HAL_StatusTypeDef status, const char *message)
{
    CmdSetError((status == HAL_TIMEOUT) ? CMD_ERROR_TIMEOUT : CMD_ERROR_FAILURE, message);
}

static void CmdSetSuccess(void)
{
    responseLength = 0U;
    responseBuffer[0] = '\0';
    (void)CmdAppend("$0000;<>");
}

static bool CmdParseNumber(uint32_t index, uint32_t maximum, uint32_t *value)
{
    char *end;
    unsigned long parsed;

    if ((index >= argumentCount) || (value == NULL) || (arguments[index][0] == '-'))
    {
        return false;
    }

    parsed = strtoul(arguments[index], &end, 0);
    if ((end == arguments[index]) || (*end != '\0') || (parsed > maximum))
    {
        return false;
    }
    *value = (uint32_t)parsed;
    return true;
}

static bool CmdRequireArgumentCount(uint32_t expected)
{
    if (argumentCount == expected)
    {
        return true;
    }
    CmdSetError(CMD_ERROR_MISSING_PARAMETER, "wrong parameter count");
    return false;
}

static bool CmdParseAddress(uint32_t index, uint8_t *address)
{
    uint32_t value;

    if (!CmdParseNumber(index, 0x7FU, &value) || (value == 0U) || (value == 0x7EU))
    {
        CmdSetError(CMD_ERROR_OUT_OF_RANGE, "invalid 7-bit address");
        return false;
    }
    *address = (uint8_t)value;
    return true;
}

static void CmdAppendBytes(const uint8_t *data, uint32_t length)
{
    uint32_t index;

    CmdSetSuccess();
    for (index = 0U; index < length; ++index)
    {
        if (!CmdAppend(" 0x%02X", data[index]))
        {
            CmdSetError(CMD_ERROR_FAILURE, "response too long");
            return;
        }
    }
}

static void CmdReadCcc(uint8_t ccc, uint32_t length, const char *failureMessage)
{
    uint8_t data[6] = {0};
    HAL_StatusTypeDef status;

    if (!CmdRequireArgumentCount(0U))
    {
        return;
    }
    status = I3cBusDirectCcc(ccc, NULL, 0U, data, length, HAL_I3C_DIRECTION_READ);
    if (status != HAL_OK)
    {
        CmdSetHalError(status, failureMessage);
        return;
    }
    CmdAppendBytes(data, length);
}

static void CmdWriteCcc(uint8_t ccc, uint32_t length, uint32_t maximum, const char *failureMessage)
{
    uint8_t data[2] = {0};
    uint32_t value;
    uint32_t index;
    HAL_StatusTypeDef status;

    if (!CmdRequireArgumentCount(length))
    {
        return;
    }
    for (index = 0U; index < length; ++index)
    {
        if (!CmdParseNumber(index, maximum, &value))
        {
            CmdSetError(CMD_ERROR_OUT_OF_RANGE, "CCC parameter out of range");
            return;
        }
        data[index] = (uint8_t)value;
    }

    status = I3cBusDirectCcc(ccc, data, length, NULL, 0U, HAL_I3C_DIRECTION_WRITE);
    if (status != HAL_OK)
    {
        CmdSetHalError(status, failureMessage);
        return;
    }
    CmdSetSuccess();
}

static void CmdHandleHelp(void)
{
    uint32_t index;

    CmdSetSuccess();
    (void)CmdAppend(" eth2i3c FreeRTOS+TCP\r\n");
    for (index = 0U; index < (sizeof(commandTable) / sizeof(commandTable[0])); ++index)
    {
        if (!CmdAppend("%s\r\n", commandTable[index].usage))
        {
            CmdSetError(CMD_ERROR_FAILURE, "help response too long");
            return;
        }
    }
}

static void CmdHandleTwi(void)
{
    uint32_t length;
    uint32_t reg;
    uint32_t value;
    uint32_t index;
    HAL_StatusTypeDef status;

    if ((argumentCount < 1U) || !CmdParseNumber(0U, I3C_BUS_MAX_DATA_BYTES, &length) || (length == 0U))
    {
        CmdSetError(CMD_ERROR_OUT_OF_RANGE, "length must be 1..2048");
        return;
    }

    if (argumentCount == 1U)
    {
        status = I3cBusPrivateTransfer(NULL, 0U, transferBuffer, length, HAL_I3C_DIRECTION_READ);
        if (status == HAL_OK)
        {
            CmdAppendBytes(transferBuffer, length);
        }
        else
        {
            CmdSetHalError(status, "current read failed");
        }
        return;
    }

    if (!CmdParseNumber(1U, UINT8_MAX, &reg))
    {
        CmdSetError(CMD_ERROR_OUT_OF_RANGE, "register out of range");
        return;
    }
    if (argumentCount == 2U)
    {
        status = I3cBusRandomRead((uint8_t)reg, transferBuffer, length);
        if (status == HAL_OK)
        {
            CmdAppendBytes(transferBuffer, length);
        }
        else
        {
            CmdSetHalError(status, "random read failed");
        }
        return;
    }

    if (argumentCount != (length + 2U))
    {
        CmdSetError(CMD_ERROR_MISSING_PARAMETER, "write data count does not match length");
        return;
    }

    transferBuffer[0] = (uint8_t)reg;
    for (index = 0U; index < length; ++index)
    {
        if (!CmdParseNumber(index + 2U, UINT8_MAX, &value))
        {
            CmdSetError(CMD_ERROR_OUT_OF_RANGE, "write byte out of range");
            return;
        }
        transferBuffer[index + 1U] = (uint8_t)value;
    }

    status = I3cBusPrivateTransfer(transferBuffer, length + 1U, NULL, 0U, HAL_I3C_DIRECTION_WRITE);
    if (status != HAL_OK)
    {
        CmdSetHalError(status, "write failed");
        return;
    }
    CmdSetSuccess();
}

static void CmdHandleRstdaa(void)
{
    HAL_StatusTypeDef status;

    if (!CmdRequireArgumentCount(0U))
    {
        return;
    }
    status = I3cBusRstdaa();
    if (status != HAL_OK)
    {
        CmdSetHalError(status, "RSTDAA failed");
        return;
    }
    CmdSetSuccess();
}

static void CmdHandleEntdaa(void)
{
    HAL_StatusTypeDef status;

    if (!CmdRequireArgumentCount(0U))
    {
        return;
    }
    status = I3cBusEntdaa();
    if (status != HAL_OK)
    {
        CmdSetHalError(status, "ENTDAA failed");
        return;
    }
    CmdSetSuccess();
    (void)CmdAppend(" %u", (unsigned)ControllerGet()->assignedCount);
}

static void CmdHandleGetpid(void)
{
    CmdReadCcc(I3C_CCC_GETPID, 6U, "GETPID failed");
}

static void CmdHandleGetbcr(void)
{
    CmdReadCcc(I3C_CCC_GETBCR, 1U, "GETBCR failed");
}

static void CmdHandleGetdcr(void)
{
    CmdReadCcc(I3C_CCC_GETDCR, 1U, "GETDCR failed");
}

static void CmdHandleGetmwl(void)
{
    CmdReadCcc(I3C_CCC_GETMWL, 2U, "GETMWL failed");
}

static void CmdHandleGetmrl(void)
{
    CmdReadCcc(I3C_CCC_GETMRL, 2U, "GETMRL failed");
}

static void CmdHandleGetstatus(void)
{
    CmdReadCcc(I3C_CCC_GETSTATUS, 2U, "GETSTATUS failed");
}

static void CmdHandleSetmwl(void)
{
    CmdWriteCcc(I3C_CCC_SETMWL, 2U, UINT8_MAX, "SETMWL failed");
}

static void CmdHandleSetmrl(void)
{
    CmdWriteCcc(I3C_CCC_SETMRL, 2U, UINT8_MAX, "SETMRL failed");
}

static void CmdHandleEnec(void)
{
    CmdWriteCcc(I3C_CCC_ENEC, 1U, 0x07U, "ENEC failed");
}

static void CmdHandleDisec(void)
{
    CmdWriteCcc(I3C_CCC_DISEC, 1U, 0x07U, "DISEC failed");
}

static void CmdHandleTargetList(void)
{
    Controller_t *controller = ControllerGet();
    uint32_t index;

    if (!CmdRequireArgumentCount(0U) || !ControllerBusAcquire(osWaitForever))
    {
        if (responseLength == 0U)
        {
            CmdSetError(CMD_ERROR_FAILURE, "controller bus unavailable");
        }
        return;
    }

    CmdSetSuccess();
    (void)CmdAppend(" %u", (unsigned)TargetCount());
    for (index = 0U; index < TARGET_MAX_COUNT; ++index)
    {
        const TargetEntry_t *entry = TargetGetEntry((uint8_t)index);
        uint32_t payloadHigh;
        uint32_t payloadLow;

        if ((entry == NULL) || ((entry->sa == 0U) && (entry->da == 0U)))
        {
            continue;
        }
        payloadHigh = (uint32_t)(entry->payload >> 32U);
        payloadLow = (uint32_t)entry->payload;
        if (!CmdAppend(" 0x%02X 0x%02X %u %u 0x%08lX%08lX", entry->sa, entry->da,
                       (unsigned)entry->protocol, (unsigned)(entry == controller->activeTarget),
                       (unsigned long)payloadHigh, (unsigned long)payloadLow))
        {
            ControllerBusRelease();
            CmdSetError(CMD_ERROR_FAILURE, "target list response too long");
            return;
        }
    }
    ControllerBusRelease();
}

static void CmdHandleTargetCreateBySa(void)
{
    uint8_t sa;

    if (!CmdRequireArgumentCount(1U) || !CmdParseAddress(0U, &sa))
    {
        return;
    }
    if (!ControllerBusAcquire(osWaitForever))
    {
        CmdSetError(CMD_ERROR_FAILURE, "controller bus unavailable");
        return;
    }
    if (TargetCreateBySa(sa) == NULL)
    {
        ControllerBusRelease();
        CmdSetError(CMD_ERROR_FAILURE, "target create failed");
        return;
    }
    ControllerBusRelease();
    CmdSetSuccess();
}

static void CmdSetActive(bool byDynamicAddress)
{
    uint8_t address;
    bool success;

    if (!CmdRequireArgumentCount(1U) || !CmdParseAddress(0U, &address))
    {
        return;
    }
    if (!ControllerBusAcquire(osWaitForever))
    {
        CmdSetError(CMD_ERROR_FAILURE, "controller bus unavailable");
        return;
    }
    success = byDynamicAddress ? TargetSetActiveByDa(address) : TargetSetActiveBySa(address);
    ControllerBusRelease();
    if (!success)
    {
        CmdSetError(CMD_ERROR_FAILURE, "target not found");
        return;
    }
    CmdSetSuccess();
}

static void CmdHandleTargetSetActiveBySa(void)
{
    CmdSetActive(false);
}

static void CmdHandleTargetSetActiveByDa(void)
{
    CmdSetActive(true);
}

static void CmdHandleTargetBind(void)
{
    uint8_t sa;
    uint8_t da;

    if (!CmdRequireArgumentCount(2U) || !CmdParseAddress(0U, &sa) || !CmdParseAddress(1U, &da))
    {
        return;
    }
    if (!ControllerBusAcquire(osWaitForever))
    {
        CmdSetError(CMD_ERROR_FAILURE, "controller bus unavailable");
        return;
    }
    if (TargetBind(sa, da) == NULL)
    {
        ControllerBusRelease();
        CmdSetError(CMD_ERROR_FAILURE, "target bind failed");
        return;
    }
    ControllerBusRelease();
    CmdSetSuccess();
}

static void CmdHandleTargetNegotiateBySa(void)
{
    uint8_t sa;
    uint8_t da;
    HAL_StatusTypeDef status;

    if (!CmdRequireArgumentCount(1U) || !CmdParseAddress(0U, &sa))
    {
        return;
    }
    status = I3cBusNegotiateBySa(sa, &da);
    if (status != HAL_OK)
    {
        CmdSetHalError(status, "I3C negotiation failed");
        return;
    }
    CmdSetSuccess();
    (void)CmdAppend(" 0x%02X", da);
}

static void CmdSetProtocol(bool byDynamicAddress)
{
    uint8_t address;
    uint32_t protocol;
    bool success;

    if (!CmdRequireArgumentCount(2U) || !CmdParseAddress(0U, &address) ||
        !CmdParseNumber(1U, TARGET_PROTOCOL_I3C, &protocol))
    {
        if (responseLength == 0U)
        {
            CmdSetError(CMD_ERROR_OUT_OF_RANGE, "protocol must be 0 or 1");
        }
        return;
    }
    if (!ControllerBusAcquire(osWaitForever))
    {
        CmdSetError(CMD_ERROR_FAILURE, "controller bus unavailable");
        return;
    }
    success = byDynamicAddress ? TargetSetProtocolByDa(address, (TargetProtocol_t)protocol) :
                                 TargetSetProtocolBySa(address, (TargetProtocol_t)protocol);
    ControllerBusRelease();
    if (!success)
    {
        CmdSetError(CMD_ERROR_FAILURE, "target protocol update failed");
        return;
    }
    CmdSetSuccess();
}

static void CmdHandleTargetSetProtocolBySa(void)
{
    CmdSetProtocol(false);
}

static void CmdHandleTargetSetProtocolByDa(void)
{
    CmdSetProtocol(true);
}

const char *CmdProcess(char *line)
{
    char *token;
    uint32_t index;

    responseLength = 0U;
    responseBuffer[0] = '\0';
    argumentCount = 0U;
    if (line == NULL)
    {
        CmdSetError(CMD_ERROR_INVALID, "empty command");
        return responseBuffer;
    }

    token = strtok(line, " \t,\r\n");
    if (token == NULL)
    {
        CmdSetError(CMD_ERROR_INVALID, "empty command");
        return responseBuffer;
    }
    CmdUppercase(token);

    while ((token = strtok(NULL, " \t,\r\n")) != NULL)
    {
        if (argumentCount >= CMD_MAX_ARGUMENTS)
        {
            CmdSetError(CMD_ERROR_OUT_OF_RANGE, "too many parameters");
            return responseBuffer;
        }
        arguments[argumentCount++] = token;
    }

    for (index = 0U; index < (sizeof(commandTable) / sizeof(commandTable[0])); ++index)
    {
        if (strcmp(line, commandTable[index].name) == 0)
        {
            commandTable[index].handler();
            return responseBuffer;
        }
    }

    CmdSetError(CMD_ERROR_INVALID, "invalid command");
    return responseBuffer;
}
