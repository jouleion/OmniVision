#define configUSE_PREEMPTION        1
#define configUSE_IDLE_HOOK         0
#define configUSE_TICK_HOOK         0
#define configCPU_CLOCK_HZ          (133000000UL)  ; 133MHz RP2040
#define configTICK_RATE_HZ          ((TickType_t)1000)
#define configMAX_PRIORITIES        (5)
#define configMINIMAL_STACK_SIZE    ((uint16_t)128)
#define configTOTAL_HEAP_SIZE       ((size_t)(200 * 1024))  ; 200KB heap
#define configMAX_TASK_NAME_LEN     (16)
#define configUSE_16_BIT_TICKS      0
#define configIDLE_SHOULD_YIELD     1

// #include "pico/multicore.h"  ; For dual-core support
