#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#if defined(__ICCARM__) || defined(__GNUC__)
#include "stm32h563xx.h"

#include <stdint.h>
#else
#define __NVIC_PRIO_BITS 4U
#endif

#define CMSIS_device_header "stm32h563xx.h"

#define configUSE_PREEMPTION 1
#define configUSE_TIME_SLICING 1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configCPU_CLOCK_HZ SystemCoreClock
#define configTICK_RATE_HZ ((TickType_t)1000U)
#define configMAX_PRIORITIES 56U
#define configMINIMAL_STACK_SIZE ((uint16_t)256U)
#define configMAX_TASK_NAME_LEN 16U
#define configUSE_16_BIT_TICKS 0
#define configIDLE_SHOULD_YIELD 1
#define configUSE_TASK_NOTIFICATIONS 1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES 1U
#define configUSE_MUTEXES 1
#define configUSE_RECURSIVE_MUTEXES 1
#define configUSE_COUNTING_SEMAPHORES 1
#define configUSE_QUEUE_SETS 0
#define configQUEUE_REGISTRY_SIZE 8U
#define configUSE_APPLICATION_TASK_TAG 0
#define configUSE_TRACE_FACILITY 0
#define configUSE_STATS_FORMATTING_FUNCTIONS 0
#define configGENERATE_RUN_TIME_STATS 0

#define configSUPPORT_STATIC_ALLOCATION 1
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configKERNEL_PROVIDED_STATIC_MEMORY 1
#define configAPPLICATION_ALLOCATED_HEAP 0
#define configTOTAL_HEAP_SIZE ((size_t)(128U * 1024U))

#define configUSE_TIMERS 1
#define configTIMER_TASK_PRIORITY 3U
#define configTIMER_QUEUE_LENGTH 8U
#define configTIMER_TASK_STACK_DEPTH 512U

#define configUSE_IDLE_HOOK 0
#define configUSE_TICK_HOOK 0
#define configUSE_MALLOC_FAILED_HOOK 1
#define configCHECK_FOR_STACK_OVERFLOW 2
#define configUSE_DAEMON_TASK_STARTUP_HOOK 0
#define configUSE_TICKLESS_IDLE 0

#define configENABLE_FPU 1
#define configENABLE_MPU 0
#define configENABLE_TRUSTZONE 0
#define configRUN_FREERTOS_SECURE_ONLY 0
#define configENABLE_PAC 0
#define configENABLE_BTI 0

#define configKERNEL_INTERRUPT_PRIORITY (15U << (8U - __NVIC_PRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (5U << (8U - __NVIC_PRIO_BITS))

/* Let the CMSIS-RTOS2 wrapper own SysTick_Handler and call the renamed port handler. */
#define SysTick_Handler xPortSysTickHandler

#define configUSE_OS2_THREAD_SUSPEND_RESUME 0
#define configUSE_OS2_THREAD_ENUMERATE 0
#define configUSE_OS2_EVENTFLAGS_FROM_ISR 0
#define configUSE_OS2_THREAD_FLAGS 1
#define configUSE_OS2_TIMER 1
#define configUSE_OS2_MUTEX 1

#define INCLUDE_vTaskPrioritySet 1
#define INCLUDE_uxTaskPriorityGet 1
#define INCLUDE_vTaskDelete 1
#define INCLUDE_vTaskSuspend 0
#define INCLUDE_vTaskDelay 1
#define INCLUDE_xTaskDelayUntil 1
#define INCLUDE_xTaskGetSchedulerState 1
#define INCLUDE_xTaskGetCurrentTaskHandle 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define INCLUDE_xSemaphoreGetMutexHolder 1
#define INCLUDE_eTaskGetState 1
#define INCLUDE_xTimerPendFunctionCall 1
#define INCLUDE_xTaskAbortDelay 1

/* CMSIS-FreeRTOS 11.3 uses this CMSIS 6 attribute; STM32Cube H5 provides CMSIS-RTOS2 V2.1.3. */
#ifndef osThreadPrivileged
#define osThreadPrivileged 0x00000004U
#endif

#define configASSERT(expression)                 \
    do                                           \
    {                                            \
        if ((expression) == 0)                   \
        {                                        \
            __disable_irq();                     \
            for (;;)                             \
            {                                    \
            }                                    \
        }                                        \
    } while (0)

#endif
