/*
 * kernel.c
 *
 * Erstellt am: 27. Apr 2026
 * Autor: Danielou Mounsande
 */

#include "kernel.h"
#include <string.h>
#include "main.h"
#include "tcb.h"

/* --- Abschnitt 1: Hardware-Definitionen (Cortex-M4) --- */

#define INTERRUPT_CONTROL_STATE_REGISTER_ADDRESS                             ( 0xE000ED04u )
#define PENDSV_SET_BIT_MASK                                                  ( 1u << 28 )
#define SYSTEM_HANDLER_PRIORITY_REGISTER_3_ADDRESS                           ( 0xE000ED22u )
#define PENDSV_PRIORITY_LOWEST                                               ( 0xFFu )
#define COPROCESSOR_ACCESS_CONTROL_REGISTER_ADDRESS                          ( 0xE000ED88u )
#define CORTEX_M4_INITIAL_xPSR_VALUE                                         ( 1u << 24 )

/* --- Abschnitt 2: Interner Zustand des Kernels --- */

static TCB_sctTCB_t Global_ArrayOfAllTCBs[KERNEL_MAXIMUM_NUMBER_OF_TASKS] __attribute__((aligned(8)));
static uint8_t Global_TotalNumberOfCreatedTasks = 0;
static uint8_t Global_IndexDerAktuellenTask = 0;
static uint8_t Global_OsIsRunning = 0; /* Sicherheitsverriegelung beim Start */

TCB_sctTCB_t * volatile Global_PointerToCurrentlyRunningTCB = NULL;


/* --- Abschnitt 3: Interne Funktionen und Hooks --- */

/* Sicherheitsnetz: Wenn ein Task versucht ein "return" auszuführen oder beendet wird,
 * landet er hier, anstatt einen HardFault bei Adresse 0x00000000 auszulösen.
 */
static void Kernel_TaskReturnHook(void)
{
    while(1)
    {
        /* Wenn der Debugger hier anhält, hat einer der Tasks keine while(1)-Schleife! */
        __asm volatile ("nop");
    }
}

/* Standard-Task, um die CPU auszulasten, wenn nichts anderes läuft */
static void Kernel_IdleTask(void)
{
    while(1)
    {
        __asm volatile ("nop");
    }
}

/* Intelligenter Scheduler: nimmt den alten Stack-Pointer, gibt den neuen zurück! */
uint32_t Kernel_ContextSwitch(uint32_t current_sp)
{
    /* DEBUG: LED3 (PC9) bei jedem Kontextwechsel umschalten, um zu beweisen, dass der Kernel lebt */
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_9);

    /* 1. Sichert den SP des ausgehenden Tasks */
    if (Global_PointerToCurrentlyRunningTCB != NULL)
    {
        Global_PointerToCurrentlyRunningTCB->u32TaskSP = current_sp;
    }

    /* 2. Auswahl des nächsten Tasks (Round Robin) */
    Global_IndexDerAktuellenTask++;
    if (Global_IndexDerAktuellenTask >= Global_TotalNumberOfCreatedTasks)
    {
        Global_IndexDerAktuellenTask = 0;
    }

    /* 3. Lädt den neuen Task */
    Global_PointerToCurrentlyRunningTCB = &Global_ArrayOfAllTCBs[Global_IndexDerAktuellenTask];
    Global_PointerToCurrentlyRunningTCB->eTaskState = TaskState_Running;

    /* 4. Gibt den exakten Stack-Pointer zurück */
    return Global_PointerToCurrentlyRunningTCB->u32TaskSP;
}

/* --- Abschnitt 3: Funktionen der Kernel-API --- */

void Kernel_InitializeHardwareAndTCBStructures(void)
{
    /* FPU (mathematischer Koprozessor) deaktivieren*/
    volatile uint32_t *cpacr = (volatile uint32_t *)COPROCESSOR_ACCESS_CONTROL_REGISTER_ADDRESS;
    *cpacr &= ~(0xFu << 20);

    /* Priorität von PendSV auf den niedrigstmöglichen Wert setzen */
    volatile uint8_t *shpr3 = (volatile uint8_t *)SYSTEM_HANDLER_PRIORITY_REGISTER_3_ADDRESS;
    *shpr3 = PENDSV_PRIORITY_LOWEST;

    memset(Global_ArrayOfAllTCBs, 0, sizeof(Global_ArrayOfAllTCBs));
    Global_TotalNumberOfCreatedTasks = 0;
    Global_PointerToCurrentlyRunningTCB = NULL;

    /* Automatische Erstellung des Idle-Tasks an Position 0 */
    Kernel_CreateNewTask(Kernel_IdleTask);
}

Kernel_ErrorStatus_Enumeration_t Kernel_CreateNewTask(Kernel_TaskEntryPointFunctionPointer_t taskFunctionPointer)
{
    if (Global_TotalNumberOfCreatedTasks >= KERNEL_MAXIMUM_NUMBER_OF_TASKS) return KernelError_TCBArrayIsFull;
    if (taskFunctionPointer == NULL) return KernelError_InvalidParameterProvided;

    TCB_sctTCB_t *pTCB = &Global_ArrayOfAllTCBs[Global_TotalNumberOfCreatedTasks];
    pTCB->eTaskState = TaskState_Ready;

    /* 1. Platzierung des Stack-Pointers am ENDE des Arrays (Full Descending) */
    uint32_t *sp = &pTCB->au32TaskStack[TCB_TASK_STACK_SIZE];

    /* 2. Strikte Ausrichtung auf 8 Bytes (ARM-Hardwareanforderung) */
    sp = (uint32_t *)(((uint32_t)sp) & 0xFFFFFFF8u);

    /* 3. Gefälschter Stack-Frame (Dummy Stack Frame) */
    *(--sp) = CORTEX_M4_INITIAL_xPSR_VALUE;                  /* xPSR (Bit 24 zwingend auf 1) */
    *(--sp) = (uint32_t)taskFunctionPointer | 0x01u;         /* PC (Program Counter) + Thumb-Bit */
    *(--sp) = (uint32_t)Kernel_TaskReturnHook | 0x01u;       /* LR (Link Register) + Thumb-Bit */
    *(--sp) = 0x12121212u;                                   /* R12 */
    *(--sp) = 0x03030303u;                                   /* R3 */
    *(--sp) = 0x02020202u;                                   /* R2 */
    *(--sp) = 0x01010101u;                                   /* R1 */
    *(--sp) = 0x00000000u;                                   /* R0 (Parameter der Funktion) */

    /* 4. Sicherung der restlichen Register (R4 bis R11) */
    *(--sp) = 0x11111111u; /* R11 */
    *(--sp) = 0x10101010u; /* R10 */
    *(--sp) = 0x09090909u; /* R9 */
    *(--sp) = 0x08080808u; /* R8 */
    *(--sp) = 0x07070707u; /* R7 */
    *(--sp) = 0x06060606u; /* R6 */
    *(--sp) = 0x05050505u; /* R5 */
    *(--sp) = 0x04040404u; /* R4 */

    /* 5. Wir speichern die aktuelle Position des Pointers im TCB */
    pTCB->u32TaskSP = (uint32_t)sp;

    Global_TotalNumberOfCreatedTasks++;

    return KernelError_NoErrorMessage;
}

void Kernel_StartScheduling(void)
{
    if (Global_TotalNumberOfCreatedTasks > 0)
    {
        Global_OsIsRunning = 1;        /* ENTSPERRUNG: Das System ist bereit! */
        Global_PointerToCurrentlyRunningTCB = NULL;
        __enable_irq();                /* Wir stellen sicher, dass die Interrupts aktiviert sind */
        Kernel_RequestContextSwitch(); /* Löst den ersten Kontextwechsel aus */
        while(1);                      /* Wir dürfen hier niemals herauskommen */
    }
}

void Kernel_RequestContextSwitch(void)
{
    /* Wir ignorieren die Anfrage, wenn das OS noch nicht gestartet ist (SysTick-Sicherheit) */
    if (Global_OsIsRunning == 1)
    {
        volatile uint32_t *icsr = (volatile uint32_t *)INTERRUPT_CONTROL_STATE_REGISTER_ADDRESS;
        *icsr = PENDSV_SET_BIT_MASK;
    }
}

__attribute__((naked)) void PendSV_Handler(void)
{
    __asm volatile (
        "cpsid i\n"              /* Deaktiviert die Interrupts */

        "mrs r0, msp\n"          /* R0 = aktueller MSP (Wird das 1. Argument für die C-Funktion sein!) */

        /* Sichern des Software-Kontexts */
        "stmdb r0!, {r4-r11}\n"  /* Legt R4-R11 auf den Stack (R0 wird aktualisiert) */
        "msr msp, r0\n"          /* SPERREN des Stacks im Speicher */
        /* C-Aufruf mit strikter Einhaltung der ARM-Architektur (AAPCS) */
        "push {r3, lr}\n"        /* 8-Byte-Ausrichtung + Sichern des Rückgabecodes */
        "bl Kernel_ContextSwitch\n" /* Ruft C auf. R0 enthält 'current_sp'. C gibt 'new_sp' in R0 zurück! */
        "pop {r3, lr}\n"         /* Stellt den Rückgabecode wieder her */
        /* Wiederherstellung des neuen Kontexts */
        "ldmia r0!, {r4-r11}\n"  /* R0 enthält den neuen Stack. Wir holen R4-R11. */
        "msr msp, r0\n"          /* Aktualisiert den Hardware-Pointer! */
        "isb\n"                  /* Leert die Pipeline */

        "cpsie i\n"              /* Reaktiviert die Interrupts */
        "bx lr\n"                /* Kehrt erfolgreich in den Task zurück! */
        ".align 4\n"
    );
}
