/* SPDX-License-Identifier: Apache-2.0
 *
 * FreeRTOS configuration for VP_FX1_Full_SoC (Bremen rv32, M-mode, single hart).
 *
 * This header is included by assembly (portASM.S -> portContext.h) as well as
 * C, so everything outside the __ASSEMBLER__ guard must stay preprocessor-only
 * with plain numeric literals (no 'u' suffixes, no casts).
 *
 * CLINT unit contract (docs/peripheral_memory_map.md): mtime/mtimecmp advance
 * in MICROSECONDS, not CPU cycles. configCPU_CLOCK_HZ therefore expresses the
 * timer rate (1 MHz), giving 1000 us per tick at configTICK_RATE_HZ = 1000.
 * main.c static-asserts the addresses below against soc_memory_map.h.
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* CDC_CLINT_BASE + CDC_CLINT_MTIME / CDC_CLINT_MTIMECMP */
#define configMTIME_BASE_ADDRESS        ( 0x0200BFF8 )
#define configMTIMECMP_BASE_ADDRESS     ( 0x02004000 )

#define configISR_STACK_SIZE_WORDS      ( 512 )

#define configUSE_PREEMPTION            1
#define configUSE_TIME_SLICING          1
#define configUSE_IDLE_HOOK             1
#define configUSE_TICK_HOOK             0
#define configCPU_CLOCK_HZ              ( 1000000 )
#define configTICK_RATE_HZ              ( 1000 )
#define configMAX_PRIORITIES            ( 8 )
#define configMINIMAL_STACK_SIZE        ( 256 )
#define configTOTAL_HEAP_SIZE           ( 64 * 1024 )
#define configMAX_TASK_NAME_LEN         ( 12 )
#define configUSE_16_BIT_TICKS          0
#define configIDLE_SHOULD_YIELD         1

#define configUSE_MUTEXES               1
#define configUSE_RECURSIVE_MUTEXES     0
#define configUSE_COUNTING_SEMAPHORES   1
#define configUSE_TASK_NOTIFICATIONS    1
#define configQUEUE_REGISTRY_SIZE       0

#define configSUPPORT_STATIC_ALLOCATION 0
#define configSUPPORT_DYNAMIC_ALLOCATION 1

#define configCHECK_FOR_STACK_OVERFLOW  2
#define configUSE_MALLOC_FAILED_HOOK    1

#define configUSE_TIMERS                0
#define configUSE_TRACE_FACILITY        0
#define configGENERATE_RUN_TIME_STATS   0

#define INCLUDE_vTaskPrioritySet        1
#define INCLUDE_uxTaskPriorityGet       1
#define INCLUDE_vTaskDelete             1
#define INCLUDE_vTaskSuspend            1
#define INCLUDE_vTaskDelay              1
#define INCLUDE_vTaskDelayUntil         1
#define INCLUDE_xTaskGetSchedulerState  1
#define INCLUDE_xTaskGetCurrentTaskHandle 1

#ifndef __ASSEMBLER__
void vAssertCalled( const char * pcFile, int iLine );
#define configASSERT( x )                          \
    do {                                           \
        if( ( x ) == 0 ) {                         \
            vAssertCalled( __FILE__, __LINE__ );   \
        }                                          \
    } while( 0 )
#endif /* __ASSEMBLER__ */

#endif /* FREERTOS_CONFIG_H */
