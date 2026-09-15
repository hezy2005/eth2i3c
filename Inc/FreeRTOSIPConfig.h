#ifndef FREERTOS_IP_CONFIG_H
#define FREERTOS_IP_CONFIG_H

#include <stdio.h>

#define ipconfigUSE_IPv4 1
#define ipconfigUSE_IPv6 0
#define ipconfigIPv4_BACKWARD_COMPATIBLE 1
#define ipconfigBYTE_ORDER pdFREERTOS_LITTLE_ENDIAN

#define ipconfigUSE_TCP 1
#define ipconfigUSE_TCP_WIN 1
#define ipconfigUSE_UDP 0
#define ipconfigUSE_DHCP 0
#define ipconfigUSE_DNS 0
#define ipconfigUSE_DNS_CACHE 0
#define ipconfigUSE_LLMNR 0
#define ipconfigUSE_NBNS 0
#define ipconfigUSE_MDNS 0

#define ipconfigNETWORK_MTU 1500U
#define ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS 24U
#define ipconfigEVENT_QUEUE_LENGTH 32U
#define ipconfigIP_TASK_PRIORITY (configMAX_PRIORITIES - 2U)
#define ipconfigIP_TASK_STACK_SIZE_WORDS (configMINIMAL_STACK_SIZE * 5U)
#define ipconfigARP_CACHE_ENTRIES 8U
#define ipconfigMAX_ARP_RETRANSMISSIONS 5U
#define ipconfigMAX_ARP_AGE 150U

#define ipconfigZERO_COPY_TX_DRIVER 1
#define ipconfigZERO_COPY_RX_DRIVER 1
#define ipconfigUSE_LINKED_RX_MESSAGES 1
/* STM32H5 TX checksum insertion produced SYN+ACK frames that the peer did not accept. */
#define ipconfigDRIVER_INCLUDED_TX_IP_CHECKSUM 0
#define ipconfigDRIVER_INCLUDED_RX_IP_CHECKSUM 1
#define ipconfigETHERNET_DRIVER_FILTERS_FRAME_TYPES 1
#define ipconfigETHERNET_DRIVER_FILTERS_PACKETS 0
#define ipconfigFILTER_OUT_NON_ETHERNET_II_FRAMES 1
#define ipconfigPACKET_FILLER_SIZE 2U
#define ipconfigPORT_SUPPRESS_WARNING 1

#define ipconfigUSE_NETWORK_EVENT_HOOK 1
#define ipconfigSUPPORT_NETWORK_DOWN_EVENT 1
#define ipconfigREPLY_TO_INCOMING_PINGS 1
#define ipconfigSUPPORT_OUTGOING_PINGS 0
#define ipconfigALLOW_SOCKET_SEND_WITHOUT_BIND 0
#define ipconfigINCLUDE_FULL_INET_ADDR 0
#define ipconfigCHECK_IP_QUEUE_SPACE 1

#define ipconfigSOCK_DEFAULT_RECEIVE_BLOCK_TIME portMAX_DELAY
#define ipconfigSOCK_DEFAULT_SEND_BLOCK_TIME pdMS_TO_TICKS(5000U)
#define ipconfigUDP_MAX_SEND_BLOCK_TIME_TICKS pdMS_TO_TICKS(5000U)
#define ipconfigTCP_RX_BUFFER_LENGTH 4096U
#define ipconfigTCP_TX_BUFFER_LENGTH 4096U
#define ipconfigTCP_WIN_SEG_COUNT 32U
#define ipconfigTCP_TIME_TO_LIVE 128U
#define ipconfigTCP_KEEP_ALIVE 1
#define ipconfigTCP_KEEP_ALIVE_INTERVAL 20U

#define ipconfigHAS_PRINTF 0
#if ipconfigHAS_PRINTF
#define FreeRTOS_printf(arguments) printf arguments
#endif

#define ipconfigHAS_DEBUG_PRINTF 0
#if ipconfigHAS_DEBUG_PRINTF
#define FreeRTOS_debug_printf(arguments) printf arguments
#endif

#if defined(__ICCARM__)
#define ipconfigISO_STRICTNESS_VIOLATION_START
#define ipconfigISO_STRICTNESS_VIOLATION_END
#else
#define ipconfigISO_STRICTNESS_VIOLATION_START _Pragma("GCC diagnostic push")
#define ipconfigISO_STRICTNESS_VIOLATION_END _Pragma("GCC diagnostic pop")
#endif

#endif
