#include "tcp_server.h"

#include "FreeRTOS.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "cmsis_os2.h"
#include "cmd.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TCP_SERVER_PORT 1000U
#define TCP_SERVER_BACKLOG 1
#define TCP_RECEIVE_CHUNK_LENGTH 512U

static char commandBuffer[CMD_INPUT_BUFFER_LENGTH];

static BaseType_t TcpServerSendAll(Socket_t socket, const char *data, size_t length)
{
    size_t sent = 0U;

    while (sent < length)
    {
        BaseType_t result = FreeRTOS_send(socket, &data[sent], length - sent, 0);

        if (result <= 0)
        {
            return pdFAIL;
        }
        sent += (size_t)result;
    }
    return pdPASS;
}

static BaseType_t TcpServerSendResponse(Socket_t socket, char *command)
{
    static const char lineEnding[] = "\r\n";
    const char *response = CmdProcess(command);

    if (TcpServerSendAll(socket, response, strlen(response)) != pdPASS)
    {
        return pdFAIL;
    }
    return TcpServerSendAll(socket, lineEnding, sizeof(lineEnding) - 1U);
}

static BaseType_t TcpServerServeClient(Socket_t clientSocket)
{
    static const char welcome[] = "eth2i3c FreeRTOS+TCP ready\r\n";
    char receiveBuffer[TCP_RECEIVE_CHUNK_LENGTH];
    size_t commandLength = 0U;
    bool discarding = false;

    if (TcpServerSendAll(clientSocket, welcome, sizeof(welcome) - 1U) != pdPASS)
    {
        return pdFAIL;
    }

    while (true)
    {
        BaseType_t received = FreeRTOS_recv(clientSocket, receiveBuffer, sizeof(receiveBuffer), 0);
        BaseType_t index;

        if (received == FREERTOS_EWOULDBLOCK)
        {
            /* An idle client is still connected; the configured receive timeout expired. */
            continue;
        }
        if (received <= 0)
        {
            if (received < 0)
            {
                printf("TCP receive failed: %ld\r\n", (long)received);
            }
            return pdFAIL;
        }

        for (index = 0; index < received; ++index)
        {
            char value = receiveBuffer[index];

            if (value == '\n')
            {
                if (discarding)
                {
                    static const char overflowResponse[] = "$0040;<> command too long\r\n";

                    if (TcpServerSendAll(clientSocket, overflowResponse, sizeof(overflowResponse) - 1U) != pdPASS)
                    {
                        return pdFAIL;
                    }
                }
                else if (commandLength != 0U)
                {
                    commandBuffer[commandLength] = '\0';
                    if (TcpServerSendResponse(clientSocket, commandBuffer) != pdPASS)
                    {
                        return pdFAIL;
                    }
                }
                commandLength = 0U;
                discarding = false;
                continue;
            }

            if (discarding)
            {
                continue;
            }
            if (commandLength >= (sizeof(commandBuffer) - 1U))
            {
                commandLength = 0U;
                discarding = true;
                continue;
            }
            commandBuffer[commandLength++] = value;
        }
    }
}

void TcpServerTask(void *argument)
{
    struct freertos_sockaddr bindAddress = {0};
    BaseType_t result;
    Socket_t serverSocket;

    (void)argument;
    serverSocket = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP);
    if (serverSocket == FREERTOS_INVALID_SOCKET)
    {
        printf("Failed: FreeRTOS_socket\r\n");
        osThreadExit();
    }

    bindAddress.sin_family = FREERTOS_AF_INET;
    bindAddress.sin_port = FreeRTOS_htons(TCP_SERVER_PORT);
    bindAddress.sin_address.ulIP_IPv4 = 0U;
    result = FreeRTOS_bind(serverSocket, &bindAddress, sizeof(bindAddress));
    if (result != 0)
    {
        printf("Failed: FreeRTOS_bind returned %ld\r\n", (long)result);
        FreeRTOS_closesocket(serverSocket);
        osThreadExit();
    }

    result = FreeRTOS_listen(serverSocket, TCP_SERVER_BACKLOG);
    if (result != 0)
    {
        printf("Failed: FreeRTOS_listen returned %ld\r\n", (long)result);
        FreeRTOS_closesocket(serverSocket);
        osThreadExit();
    }

    printf("TCP Server listening on PORT %u.. \r\n", TCP_SERVER_PORT);
    while (true)
    {
        char clientIp[16];
        struct freertos_sockaddr clientAddress = {0};
        socklen_t clientAddressLength = sizeof(clientAddress);
        Socket_t clientSocket = FreeRTOS_accept(serverSocket, &clientAddress, &clientAddressLength);

        if ((clientSocket == FREERTOS_INVALID_SOCKET) || (clientSocket == NULL))
        {
            continue;
        }

        (void)FreeRTOS_inet_ntoa(clientAddress.sin_address.ulIP_IPv4, clientIp);
        printf("TCP client connected: %s:%u\r\n", clientIp, (unsigned int)FreeRTOS_ntohs(clientAddress.sin_port));
        (void)TcpServerServeClient(clientSocket);
        FreeRTOS_shutdown(clientSocket, FREERTOS_SHUT_RDWR);
        FreeRTOS_closesocket(clientSocket);
        printf("TCP client disconnected\r\n");
    }
}
