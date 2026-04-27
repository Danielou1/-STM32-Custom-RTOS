/*
 * kernel.c
 *
 *  Created on: Apr 27, 2026
 *      Author: Danielou Mounsande
 */

#include "kernel.h"
#include <string.h>

/* --- Sektion 1: Hardware-Definitionen --- */

/** 
 * ICSR (Interrupt Control and State Register) : Kontrollregister für Interrupts.
 * Hier fordern wir den PendSV-Interrupt an.
 */
#define INTERRUPT_CONTROL_STATE_REGISTER_ADDRESS                             ( 0xE000ED04u )
#define PENDSV_SET_BIT_MASK                                                  ( 1u << 28 )

/* --- Sektion 2: Interner Kernel-Zustand --- */

static TCB_sctTCB_t Global_ArrayOfAllTCBs[KERNEL_MAXIMUM_NUMBER_OF_TASKS];
static uint8_t Global_TotalNumberOfCreatedTasks = 0;
static uint8_t Global_IndexDerAktuellenTask = 0;

/**
 * Global_PointerToCurrentlyRunningTCB:
 * Zeigt auf den TCB der aktuell ausgeführten Task.
 * Nicht 'static', damit der Assembler-Handler darauf zugreifen kann.
 */
TCB_sctTCB_t *Global_PointerToCurrentlyRunningTCB = NULL;

/**
 * Kernel_WaehleNaechsteTaskAus:
 * Ein einfacher Round-Robin-Algorithmus. Wir gehen im Kreis durch das Array.
 */
void Kernel_WaehleNaechsteTaskAus(void)
{
    /* Den Index erhöhen */
    Global_IndexDerAktuellenTask++;

    /* Wenn wir am Ende des Arrays sind oder alle Tasks durch haben, fangen wir wieder bei 0 an */
    if (Global_IndexDerAktuellenTask >= Global_TotalNumberOfCreatedTasks)
    {
        Global_IndexDerAktuellenTask = 0;
    }

    /* Den globalen Pointer auf die neue Task setzen */
    Global_PointerToCurrentlyRunningTCB = &Global_ArrayOfAllTCBs[Global_IndexDerAktuellenTask];
}

/**
 * Initialer Wert für das xPSR-Register:
 * Das T-Bit (Position 24) muss 1 sein (Thumb-Modus).
 */
#define CORTEX_M4_xPSR_THUMB_BIT_POSITION                                    ( 24u )
#define CORTEX_M4_INITIAL_xPSR_VALUE                                         ( 1u << CORTEX_M4_xPSR_THUMB_BIT_POSITION )

/**
 * EXC_RETURN Wert:
 * 0xFFFFFFFD: Rückkehr in den Thread-Modus, verwendet den MSP.
 */
#define CORTEX_M4_EXC_RETURN_THREAD_MODE_MSP                                 ( 0xFFFFFFFDu )


/* --- Sektion 3: Kernel-Funktionen --- */

void Kernel_InitializeHardwareAndTCBStructures(void)
{
    memset(Global_ArrayOfAllTCBs, 0, sizeof(Global_ArrayOfAllTCBs));
    Global_TotalNumberOfCreatedTasks = 0;
    Global_PointerToCurrentlyRunningTCB = NULL;
}

Kernel_ErrorStatus_Enumeration_t Kernel_CreateNewTask(Kernel_TaskEntryPointFunctionPointer_t taskFunctionPointer, uint8_t taskPriority)
{
    if (Global_TotalNumberOfCreatedTasks >= KERNEL_MAXIMUM_NUMBER_OF_TASKS)
    {
        return KernelError_TCBArrayIsFull;
    }

    if (taskFunctionPointer == NULL)
    {
        return KernelError_InvalidParameterProvided;
    }

    TCB_sctTCB_t *pointerToNewTCB = &Global_ArrayOfAllTCBs[Global_TotalNumberOfCreatedTasks];
    
    pointerToNewTCB->u8TaskId = Global_TotalNumberOfCreatedTasks;
    pointerToNewTCB->u8TaskPrio = taskPriority;
    pointerToNewTCB->eTaskState = TaskState_Ready;

    /* Stack-Initialisierung: Stack wächst nach unten */
    uint32_t *stackPointerIterator = &pointerToNewTCB->au32TaskStack[TCB_TASK_STACK_SIZE];

    /* Hardware Stack Frame (vom Prozessor automatisch geladen) */
    *(--stackPointerIterator) = CORTEX_M4_INITIAL_xPSR_VALUE;               /* xPSR */
    *(--stackPointerIterator) = (uint32_t)taskFunctionPointer;              /* PC (Program Counter) */
    *(--stackPointerIterator) = CORTEX_M4_EXC_RETURN_THREAD_MODE_MSP;       /* LR (Link Register) */
    *(--stackPointerIterator) = 0x12121212u;                                /* R12 */
    *(--stackPointerIterator) = 0x03030303u;                                /* R3 */
    *(--stackPointerIterator) = 0x02020202u;                                /* R2 */
    *(--stackPointerIterator) = 0x01010101u;                                /* R1 */
    *(--stackPointerIterator) = 0x00000000u;                                /* R0 */

    /* Software Stack Frame (muss manuell vom Scheduler gerettet werden) */
    *(--stackPointerIterator) = 0x11111111u;                                /* R11 */
    *(--stackPointerIterator) = 0x10101010u;                                /* R10 */
    *(--stackPointerIterator) = 0x09090909u;                                /* R9 */
    *(--stackPointerIterator) = 0x08080808u;                                /* R8 */
    *(--stackPointerIterator) = 0x07070707u;                                /* R7 */
    *(--stackPointerIterator) = 0x06060606u;                                /* R6 */
    *(--stackPointerIterator) = 0x05050505u;                                /* R5 */
    *(--stackPointerIterator) = 0x04040404u;                                /* R4 */

    pointerToNewTCB->u32TaskSP = (uint32_t)stackPointerIterator;

    Global_TotalNumberOfCreatedTasks++;
    return KernelError_NoErrorMessage;
}

void Kernel_StartScheduling(void)
{
    if (Global_TotalNumberOfCreatedTasks > 0)
    {
        Global_PointerToCurrentlyRunningTCB = &Global_ArrayOfAllTCBs[0];
        Global_PointerToCurrentlyRunningTCB->eTaskState = TaskState_Running;

        uint32_t initialStackPointer = Global_PointerToCurrentlyRunningTCB->u32TaskSP;

        __asm volatile (
            "msr msp, %0\n"
            "ldmia sp!, {r4-r11}\n"
            "ldmia sp!, {r0-r3}\n"
            "ldmia sp!, {r12, lr}\n"
            "ldmia sp!, {r1, r2}\n"
            "bx r1\n"
            : : "r" (initialStackPointer) : "memory"
        );
    }
    
    while(1);
}

void Kernel_RequestContextSwitch(void)
{
    volatile uint32_t *interruptControlRegister = (volatile uint32_t *)INTERRUPT_CONTROL_STATE_REGISTER_ADDRESS;
    *interruptControlRegister = PENDSV_SET_BIT_MASK;
}
