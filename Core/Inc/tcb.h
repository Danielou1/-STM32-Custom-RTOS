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
 * Vereinfachter TCB für HardFault/BusFault-Debugging.
 * Der Stack Pointer MUSS das erste Mitglied sein (Offset 0).
 */
typedef struct
{
    uint32_t u32TaskSP;                          /* Offset 0: Aktueller Stack Pointer */
    TCB_eTastStates_t eTaskState;                /* Offset 4: Zustand der Task */
    uint32_t u32TicksToWait;                     /* Offset 8: Verbleibende Zeit im Blocked-State */
    void *pWaitingObject;                        /* Offset 12: Objekt, auf das die Task wartet (z.B. ein Semaphor) */
    uint32_t au32TaskStack[TCB_TASK_STACK_SIZE]; /* Offset 16: Eigener Stack-Bereich */
} __attribute__((aligned(8))) TCB_sctTCB_t;

#endif /* DOS_INC_TCB_H_ */