#include "app_freertos.h"

#include "FreeRTOS.h"
#include "FreeRTOS_IP.h"
#include "cmsis_os2.h"
#include "main.h"
#include "tcp_server.h"

#include <stdint.h>
#include <stdio.h>

#define APP_IP_ADDRESS_0 192U
#define APP_IP_ADDRESS_1 168U
#define APP_IP_ADDRESS_2 1U
#define APP_IP_ADDRESS_3 212U

static const uint8_t ipAddress[4] =
{
    APP_IP_ADDRESS_0, APP_IP_ADDRESS_1, APP_IP_ADDRESS_2, APP_IP_ADDRESS_3
};
static const uint8_t netMask[4] = {255U, 255U, 255U, 0U};
static const uint8_t gatewayAddress[4] = {192U, 168U, 1U, 1U};
static const uint8_t dnsAddress[4] = {192U, 168U, 1U, 1U};
static const uint8_t macAddress[6] = {0x02U, 0x80U, 0xE1U, 0x00U, 0x00U, 0x01U};

bool AppFreertosInit(void)
{
    static const osThreadAttr_t tcpThreadAttributes =
    {
        .name = "TcpServer",
        .priority = osPriorityNormal,
        .stack_size = 4096U
    };
    osThreadId_t tcpThread;

    if (FreeRTOS_IPInit(ipAddress, netMask, gatewayAddress, dnsAddress, macAddress) != pdPASS)
    {
        return false;
    }

    tcpThread = osThreadNew(TcpServerTask, NULL, &tcpThreadAttributes);
    if (tcpThread == NULL)
    {
        return false;
    }

    return true;
}

void vApplicationIPNetworkEventHook(eIPCallbackEvent_t networkEvent)
{
    if (networkEvent == eNetworkUp)
    {
        printf("Network up: %u.%u.%u.%u\r\n", APP_IP_ADDRESS_0, APP_IP_ADDRESS_1,
               APP_IP_ADDRESS_2, APP_IP_ADDRESS_3);
    }
    else
    {
        printf("Network down\r\n");
    }
}

BaseType_t xApplicationGetRandomNumber(uint32_t *number)
{
    static uint32_t state = 0xA5C39E71U;

    if (number == NULL)
    {
        return pdFAIL;
    }
    state ^= HAL_GetTick() + 0x9E3779B9U;
    state ^= state << 13U;
    state ^= state >> 17U;
    state ^= state << 5U;
    if (state == 0U)
    {
        state = 0x6D2B79F5U;
    }
    *number = state;
    return pdPASS;
}

uint32_t ulApplicationGetNextSequenceNumber(uint32_t sourceAddress, uint16_t sourcePort,
                                            uint32_t destinationAddress, uint16_t destinationPort)
{
    uint32_t value;

    if (xApplicationGetRandomNumber(&value) != pdPASS)
    {
        value = HAL_GetTick();
    }
    return value ^ sourceAddress ^ destinationAddress ^ ((uint32_t)sourcePort << 16U) ^ destinationPort;
}

void vApplicationMallocFailedHook(void)
{
    Error_Handler();
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *taskName)
{
    (void)task;
    (void)taskName;
    Error_Handler();
}
