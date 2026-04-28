/*
 * tcb.h
 */
#ifndef DOS_INC_TCB_H_
#define DOS_INC_TCB_H_

#include <stdint.h>

#define TCB_TASK_STACK_SIZE                 ( 128u )

typedef enum
{
    TaskState_Ready = 0,
    TaskState_Running,
    TaskState_Blocked
} TCB_eTastStates_t;

/**
 * TCB Simplifié pour débogage HardFault/BusFault.
 * Le Stack Pointer DOIT être le premier membre (Offset 0).
 */
typedef struct
{
    uint32_t u32TaskSP;                          /* Offset 0 */
    TCB_eTastStates_t eTaskState;                /* Offset 4 */
    uint32_t au32TaskStack[TCB_TASK_STACK_SIZE]; /* Offset 8 */
} __attribute__((aligned(8))) TCB_sctTCB_t;

#endif /* DOS_INC_TCB_H_ */
