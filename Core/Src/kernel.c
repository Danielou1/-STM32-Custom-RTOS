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
#include "SEGGER_SYSVIEW.h"
#include <stdio.h>

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

/* Weak declarations of the test tasks to avoid linker errors if they are not defined in the current test mode */
__attribute__((weak)) void TaskLow(void) {}
__attribute__((weak)) void TaskMedium(void) {}
__attribute__((weak)) void TaskHigh(void) {}
__attribute__((weak)) void TaskProducer(void) {}
__attribute__((weak)) void TaskConsumer(void) {}

static void Kernel_IdleTask(void);

static const char* GetTaskName(Kernel_TaskEntryPointFunctionPointer_t fp) {
    if (fp == Kernel_IdleTask) return "Idle";
    if ((void*)fp == (void*)TaskLow) return "TaskLow";
    if ((void*)fp == (void*)TaskMedium) return "TaskMedium";
    if ((void*)fp == (void*)TaskHigh) return "TaskHigh";
    if ((void*)fp == (void*)TaskProducer) return "Producer";
    if ((void*)fp == (void*)TaskConsumer) return "Consumer";
    return "Unknown Task";
}

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
        SEGGER_SYSVIEW_OnTaskStopReady((uint32_t)Global_PointerToCurrentlyRunningTCB, 0);
        Global_PointerToCurrentlyRunningTCB->u32TaskSP = current_sp;
        
        /* Wenn die Task nicht blockiert ist, setzen wir sie zurück auf Ready */
        if (Global_PointerToCurrentlyRunningTCB->eTaskState == TaskState_Running)
        {
            Global_PointerToCurrentlyRunningTCB->eTaskState = TaskState_Ready;
        }
    }

    /* 2. Auswahl des nächsten Tasks (Priority-Based Scheduling mit Fairness) */
    /* Warum? Wenn mehrere Tasks die gleiche höchste Priorität haben, sollen sie 
       abwechselnd (Round-Robin) dran kommen, um Starvation zu verhindern. */
    uint8_t u8HighestPrioFound = 255;
    uint8_t u8BestTaskIndex = 0; /* IMMER mit dem Idle-Task als Fallback starten! */

    for (uint8_t i = 1; i <= Global_TotalNumberOfCreatedTasks; i++)
    {
        /* Wir starten die Suche bei der NÄCHSTEN Task nach der aktuellen (Ringpuffer-Logik) */
        uint8_t idx = (Global_IndexDerAktuellenTask + i) % Global_TotalNumberOfCreatedTasks;
        
        if (Global_ArrayOfAllTCBs[idx].eTaskState == TaskState_Ready || 
            Global_ArrayOfAllTCBs[idx].eTaskState == TaskState_Running)
        {
            /* Minimum-Suche: Wir suchen die kleinste Prio-Zahl. 
               Da wir bei 'Current + 1' starten, finden wir bei gleicher Prio automatisch die nächste Task. */
            if (Global_ArrayOfAllTCBs[idx].u8CurrentPriority < u8HighestPrioFound)
            {
                u8HighestPrioFound = Global_ArrayOfAllTCBs[idx].u8CurrentPriority;
                u8BestTaskIndex = idx;
            }
        }
    }

    /* Wir aktualisieren den globalen Index mit unserer Entscheidung */
    Global_IndexDerAktuellenTask = u8BestTaskIndex;

    /* 3. Lädt den neuen Task */
    Global_PointerToCurrentlyRunningTCB = &Global_ArrayOfAllTCBs[Global_IndexDerAktuellenTask];
    Global_PointerToCurrentlyRunningTCB->eTaskState = TaskState_Running;

    SEGGER_SYSVIEW_OnTaskStartExec((uint32_t)Global_PointerToCurrentlyRunningTCB);

    /* TeSSLa Logging fuer Kontextwechsel */
    printf("%lu: active_task = %d\r\n", HAL_GetTick(), Global_IndexDerAktuellenTask);
    if (Global_IndexDerAktuellenTask != 0) {
        printf("%lu: task_state = %d\r\n", HAL_GetTick(), Global_IndexDerAktuellenTask);
        printf("%lu: state_val = 2\r\n", HAL_GetTick()); // 2 = Running
    }

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

    // Initialize SEGGER SystemView
    SEGGER_SYSVIEW_Conf();

    memset(Global_ArrayOfAllTCBs, 0, sizeof(Global_ArrayOfAllTCBs));
    Global_TotalNumberOfCreatedTasks = 0;
    Global_PointerToCurrentlyRunningTCB = NULL;

    /* Automatische Erstellung des Idle-Tasks an Position 0 mit niedrigster Priorität */
    Kernel_CreateNewTask(Kernel_IdleTask, 255);
}

Kernel_ErrorStatus_Enumeration_t Kernel_CreateNewTask(Kernel_TaskEntryPointFunctionPointer_t taskFunctionPointer, uint8_t u8Priority)
{
    if (Global_TotalNumberOfCreatedTasks >= KERNEL_MAXIMUM_NUMBER_OF_TASKS) return KernelError_TCBArrayIsFull;
    if (taskFunctionPointer == NULL) return KernelError_InvalidParameterProvided;

    TCB_sctTCB_t *pTCB = &Global_ArrayOfAllTCBs[Global_TotalNumberOfCreatedTasks];
    pTCB->eTaskState = TaskState_Ready;
    pTCB->u32TicksToWait = 0;
    pTCB->pWaitingObject = NULL;
    
    /* Prioritäten initialisieren (Das "Warum") */
    /* BasePriority für die Wiederherstellung nach Vererbung, 
       CurrentPriority für den Scheduler */
    pTCB->u8BasePriority = u8Priority;
    pTCB->u8CurrentPriority = u8Priority;

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

    // Register task in SEGGER SystemView
    SEGGER_SYSVIEW_OnTaskCreate((uint32_t)pTCB);
    {
        SEGGER_SYSVIEW_TASKINFO info;
        memset(&info, 0, sizeof(info));
        info.TaskID = (uint32_t)pTCB;
        info.sName = GetTaskName(taskFunctionPointer);
        info.Prio = pTCB->u8BasePriority;
        info.StackBase = (uint32_t)pTCB->au32TaskStack;
        info.StackSize = sizeof(pTCB->au32TaskStack);
        SEGGER_SYSVIEW_SendTaskInfo(&info);
    }

    return KernelError_NoErrorMessage;
}

void Kernel_TaskDelay(uint32_t u32DelayInTicks)
{
    if (Global_PointerToCurrentlyRunningTCB != NULL && u32DelayInTicks > 0)
    {
        /* Kritischer Abschnitt: Task blockieren */
        Global_PointerToCurrentlyRunningTCB->u32TicksToWait = u32DelayInTicks;
        Global_PointerToCurrentlyRunningTCB->eTaskState = TaskState_Blocked;

        /* TeSSLa Logging fuer Task-Blockierung (Delay) */
        if (Global_IndexDerAktuellenTask != 0) {
            printf("%lu: task_state = %d\r\n", HAL_GetTick(), Global_IndexDerAktuellenTask);
            printf("%lu: state_val = 3\r\n", HAL_GetTick()); // 3 = Blocked
        }

        /* Sofortigen Kontextwechsel anfordern, da die Task nicht mehr laufen kann */
        Kernel_RequestContextSwitch();
    }
}

void Kernel_UpdateTimers(void)
{
    /* Gehe alle Tasks durch (außer Idle an Index 0, die schläft nie) */
    for (uint8_t i = 1; i < Global_TotalNumberOfCreatedTasks; i++)
    {
        if (Global_ArrayOfAllTCBs[i].eTaskState == TaskState_Blocked)
        {
            /* Nur wenn die Task wegen Zeit blockiert ist (Ticks > 0) */
            if (Global_ArrayOfAllTCBs[i].u32TicksToWait > 0)
            {
                Global_ArrayOfAllTCBs[i].u32TicksToWait--;

                /* Erst wenn der Timer abgelaufen ist, wecken wir sie auf */
                if (Global_ArrayOfAllTCBs[i].u32TicksToWait == 0)
                {
                    Global_ArrayOfAllTCBs[i].eTaskState = TaskState_Ready;
                    
                    /* TeSSLa Logging fuer Aufwecken (Ready) */
                    if (i != 0) {
                        printf("%lu: task_state = %d\r\n", HAL_GetTick(), i);
                        printf("%lu: state_val = 1\r\n", HAL_GetTick()); // 1 = Ready
                    }
                }
            }
            /* Wenn Ticks == 0, bedeutet das, die Task wartet auf ein Objekt (Semaphor/Mutex)
               und darf NICHT hier auf Ready gesetzt werden! */
        }
    }
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

/* --- Abschnitt 4: Semaphore-Logik --- */

void Kernel_SemaphoreInit(Kernel_Semaphore_t *pSemaphore, uint32_t u32InitialCount, uint32_t u32MaxCount)
{
    if (pSemaphore != NULL)
    {
        pSemaphore->eObjectType = KernelObjectType_Semaphore;
        pSemaphore->u32Count = u32InitialCount;
        pSemaphore->u32MaxCount = u32MaxCount;
    }
}

void Kernel_SemaphoreWait(Kernel_Semaphore_t *pSemaphore)
{
    /* Kritischer Abschnitt: Unterbrechungen deaktivieren */
    __disable_irq();

    if (pSemaphore != NULL)
    {
        if (pSemaphore->u32Count > 0)
        {
            /* Ressource verfügbar: Jeton nehmen */
            pSemaphore->u32Count--;
        }
        else
        {
            /* Ressource NICHT verfügbar: Task blockieren */
            if (Global_PointerToCurrentlyRunningTCB != NULL)
            {
                Global_PointerToCurrentlyRunningTCB->pWaitingObject = (void*)pSemaphore;
                Global_PointerToCurrentlyRunningTCB->eTaskState = TaskState_Blocked;

                /* Kontextwechsel anfordern, da diese Task warten muss */
                Kernel_RequestContextSwitch();
            }
        }
    }

    __enable_irq();
}

void Kernel_SemaphoreGive(Kernel_Semaphore_t *pSemaphore)
{
    /* Kritischer Abschnitt: Unterbrechungen deaktivieren */
    __disable_irq();

    if (pSemaphore != NULL)
    {
        /* 1. Jeton zurückgeben (bis zum Maximum) */
        if (pSemaphore->u32Count < pSemaphore->u32MaxCount)
        {
            pSemaphore->u32Count++;
        }

        /* 2. Prüfen, ob eine Task auf GENAU DIESES Semaphor wartet */
        for (uint8_t i = 0; i < Global_TotalNumberOfCreatedTasks; i++)
        {
            if (Global_ArrayOfAllTCBs[i].eTaskState == TaskState_Blocked &&
                Global_ArrayOfAllTCBs[i].pWaitingObject == (void*)pSemaphore)
            {
                /* 3. Task aufwecken */
                Global_ArrayOfAllTCBs[i].eTaskState = TaskState_Ready;
                Global_ArrayOfAllTCBs[i].pWaitingObject = NULL;

                /* 4. Kontextwechsel anfordern */
                Kernel_RequestContextSwitch();

                /* Nur eine Task pro freigegebenem Jeton aufwecken */
                break;
            }
        }
    }

    __enable_irq();
}

/* --- Abschnitt 5: Mutex-Logik --- */

void Kernel_MutexInit(Kernel_Mutex_t *pMutex)
{
    if (pMutex != NULL)
    {
        pMutex->eObjectType = KernelObjectType_Mutex;
        pMutex->pOwner = NULL;
        pMutex->u8IsLocked = 0;
    }
}

void Kernel_MutexLock(Kernel_Mutex_t *pMutex)
{
    if (pMutex == NULL) return;

    while (1)
    {
        /* Kritischer Abschnitt: Unterbrechungen deaktivieren */
        __disable_irq();

        if (pMutex->u8IsLocked == 0)
        {
            /* Mutex ist frei: Aktuelle Task wird Besitzer */
            pMutex->u8IsLocked = 1;
            pMutex->pOwner = Global_PointerToCurrentlyRunningTCB;
            
            /* TeSSLa Logging fuer Mutex Lock */
            printf("%lu: mutex_id = 1\r\n", HAL_GetTick());
            printf("%lu: mutex_task = %d\r\n", HAL_GetTick(), Global_IndexDerAktuellenTask);
            printf("%lu: mutex_action = 1\r\n", HAL_GetTick()); // 1 = Lock

            __enable_irq();
            return; /* Erfolg: Wir besitzen den Mutex */
        }
        else
        {
            /* Mutex ist besetzt: Task blockieren */
            if (Global_PointerToCurrentlyRunningTCB != NULL)
            {
                Global_PointerToCurrentlyRunningTCB->pWaitingObject = (void*)pMutex;
                Global_PointerToCurrentlyRunningTCB->eTaskState = TaskState_Blocked;

                /* TeSSLa Logging fuer Task-Blockierung auf Mutex */
                if (Global_IndexDerAktuellenTask != 0) {
                    printf("%lu: task_state = %d\r\n", HAL_GetTick(), Global_IndexDerAktuellenTask);
                    printf("%lu: state_val = 3\r\n", HAL_GetTick()); // 3 = Blocked
                }

                /* Priority Inheritance: Priorität des Mutex-Besitzers anheben */
                TCB_sctTCB_t *pOwner = pMutex->pOwner;
                if (pOwner != NULL)
                {
                    /* Höhere Priorität entspricht einer kleineren Zahl */
                    if (Global_PointerToCurrentlyRunningTCB->u8CurrentPriority < pOwner->u8CurrentPriority)
                    {
                        pOwner->u8CurrentPriority = Global_PointerToCurrentlyRunningTCB->u8CurrentPriority;

                        /* Transitive Vererbung: falls der Besitzer selbst blockiert ist */
                        TCB_sctTCB_t *pNextOwner = pOwner;
                        while (pNextOwner->pWaitingObject != NULL)
                        {
                            Kernel_ObjectHeader_t *pNextHeader = (Kernel_ObjectHeader_t *)pNextOwner->pWaitingObject;
                            if (pNextHeader->eObjectType == KernelObjectType_Mutex)
                            {
                                Kernel_Mutex_t *pNextMutex = (Kernel_Mutex_t *)pNextHeader;
                                TCB_sctTCB_t *pOwnerOfNextMutex = pNextMutex->pOwner;
                                if (pOwnerOfNextMutex != NULL && pNextOwner->u8CurrentPriority < pOwnerOfNextMutex->u8CurrentPriority)
                                {
                                    pOwnerOfNextMutex->u8CurrentPriority = pNextOwner->u8CurrentPriority;
                                    pNextOwner = pOwnerOfNextMutex;
                                }
                                else
                                {
                                    break;
                                }
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                }

                /* Kontextwechsel anfordern */
                Kernel_RequestContextSwitch();
            }
        }

        /* Nach dem Aufwecken: Interrupts kurz an und wieder von vorn prüfen */
        __enable_irq();
    }
}

void Kernel_MutexUnlock(Kernel_Mutex_t *pMutex)
{
    /* Kritischer Abschnitt: Unterbrechungen deaktivieren */
    __disable_irq();

    if (pMutex != NULL)
    {
        /* Ownership-Prüfung: Nur der Besitzer darf das Mutex freigeben! */
        if (pMutex->pOwner == Global_PointerToCurrentlyRunningTCB)
        {
            TCB_sctTCB_t *pOwner = pMutex->pOwner;

            pMutex->u8IsLocked = 0;
            pMutex->pOwner = NULL;

            /* TeSSLa Logging fuer Mutex Unlock */
            {
                uint8_t ownerIdx = 0;
                for (uint8_t j = 0; j < Global_TotalNumberOfCreatedTasks; j++) {
                    if (&Global_ArrayOfAllTCBs[j] == pOwner) {
                        ownerIdx = j;
                        break;
                    }
                }
                printf("%lu: mutex_id = 1\r\n", HAL_GetTick());
                printf("%lu: mutex_task = %d\r\n", HAL_GetTick(), ownerIdx);
                printf("%lu: mutex_action = 0\r\n", HAL_GetTick()); // 0 = Unlock
            }

            /* Prioritätsvererbung aufheben: Priorität neu berechnen */
            uint8_t u8NewPriority = pOwner->u8BasePriority;
            for (uint8_t i = 0; i < Global_TotalNumberOfCreatedTasks; i++)
            {
                if (Global_ArrayOfAllTCBs[i].eTaskState == TaskState_Blocked &&
                    Global_ArrayOfAllTCBs[i].pWaitingObject != NULL)
                {
                    Kernel_ObjectHeader_t *pHeader = (Kernel_ObjectHeader_t *)Global_ArrayOfAllTCBs[i].pWaitingObject;
                    if (pHeader->eObjectType == KernelObjectType_Mutex)
                    {
                        Kernel_Mutex_t *pWmutex = (Kernel_Mutex_t *)pHeader;
                        /* Wenn diese Task auf einen Mutex wartet, den der Besitzer hält */
                        if (pWmutex->pOwner == pOwner)
                        {
                            if (Global_ArrayOfAllTCBs[i].u8CurrentPriority < u8NewPriority)
                            {
                                u8NewPriority = Global_ArrayOfAllTCBs[i].u8CurrentPriority;
                            }
                        }
                    }
                }
            }
            pOwner->u8CurrentPriority = u8NewPriority;

            /* Die wartende Task mit der höchsten Priorität aufwecken */
            TCB_sctTCB_t *pTaskToWake = NULL;
            uint8_t u8HighestWaitingPrio = 255;
            for (uint8_t i = 0; i < Global_TotalNumberOfCreatedTasks; i++)
            {
                if (Global_ArrayOfAllTCBs[i].eTaskState == TaskState_Blocked &&
                    Global_ArrayOfAllTCBs[i].pWaitingObject == (void*)pMutex)
                {
                    if (Global_ArrayOfAllTCBs[i].u8CurrentPriority < u8HighestWaitingPrio)
                    {
                        u8HighestWaitingPrio = Global_ArrayOfAllTCBs[i].u8CurrentPriority;
                        pTaskToWake = &Global_ArrayOfAllTCBs[i];
                    }
                }
            }

            if (pTaskToWake != NULL)
            {
                /* Task bereit machen */
                pTaskToWake->eTaskState = TaskState_Ready;
                pTaskToWake->pWaitingObject = NULL;

                /* TeSSLa Logging fuer Task-Aufwecken nach Mutex-Freigabe */
                {
                    uint8_t wakeIdx = 0;
                    for (uint8_t j = 0; j < Global_TotalNumberOfCreatedTasks; j++) {
                        if (&Global_ArrayOfAllTCBs[j] == pTaskToWake) {
                            wakeIdx = j;
                            break;
                        }
                    }
                    if (wakeIdx != 0) {
                        printf("%lu: task_state = %d\r\n", HAL_GetTick(), wakeIdx);
                        printf("%lu: state_val = 1\r\n", HAL_GetTick()); // 1 = Ready
                    }
                }

                /* Kontextwechsel anfordern */
                Kernel_RequestContextSwitch();
            }
        }
    }

    __enable_irq();
}

/* --- Abschnitt 6: Message-Queue-Logik --- */

void Kernel_QueueInit(Kernel_Queue_t *pQueue, uint8_t u8Size)
{
    if (pQueue != NULL)
    {
        pQueue->eObjectType = KernelObjectType_Queue;
        pQueue->u8Head = 0;
        pQueue->u8Tail = 0;
        pQueue->u8Count = 0;
        pQueue->u8Size = (u8Size > KERNEL_QUEUE_MAX_SIZE) ? KERNEL_QUEUE_MAX_SIZE : u8Size;
        memset(pQueue->au32Buffer, 0, sizeof(pQueue->au32Buffer));
    }
}

uint8_t Kernel_QueueIsEmpty(Kernel_Queue_t *pQueue)
{
    uint8_t isEmpty = 0;
    if (pQueue != NULL)
    {
        __disable_irq();
        isEmpty = (pQueue->u8Count == 0);
        __enable_irq();
    }
    return isEmpty;
}

uint8_t Kernel_QueueIsFull(Kernel_Queue_t *pQueue)
{
    uint8_t isFull = 0;
    if (pQueue != NULL)
    {
        __disable_irq();
        isFull = (pQueue->u8Count >= pQueue->u8Size);
        __enable_irq();
    }
    return isFull;
}

Kernel_ErrorStatus_Enumeration_t Kernel_QueueSend(Kernel_Queue_t *pQueue, uint32_t u32Message, uint8_t u8Blocking)
{
    if (pQueue == NULL) return KernelError_InvalidParameterProvided;

    while (1)
    {
        __disable_irq();

        if (pQueue->u8Count < pQueue->u8Size)
        {
            /* Platz frei: in den Ringpuffer schreiben */
            pQueue->au32Buffer[pQueue->u8Head] = u32Message;
            pQueue->u8Head = (pQueue->u8Head + 1) % pQueue->u8Size;
            pQueue->u8Count++;

            /* Eine wartende Task (die auf Daten wartet) mit höchster Prio aufwecken */
            TCB_sctTCB_t *pTaskToWake = NULL;
            uint8_t u8HighestWaitingPrio = 255;
            for (uint8_t i = 0; i < Global_TotalNumberOfCreatedTasks; i++)
            {
                if (Global_ArrayOfAllTCBs[i].eTaskState == TaskState_Blocked &&
                    Global_ArrayOfAllTCBs[i].pWaitingObject == (void*)pQueue)
                {
                    if (Global_ArrayOfAllTCBs[i].u8CurrentPriority < u8HighestWaitingPrio)
                    {
                        u8HighestWaitingPrio = Global_ArrayOfAllTCBs[i].u8CurrentPriority;
                        pTaskToWake = &Global_ArrayOfAllTCBs[i];
                    }
                }
            }

            if (pTaskToWake != NULL)
            {
                pTaskToWake->eTaskState = TaskState_Ready;
                pTaskToWake->pWaitingObject = NULL;
                Kernel_RequestContextSwitch();
            }

            __enable_irq();
            return KernelError_NoErrorMessage;
        }
        else
        {
            /* Puffer voll */
            if (u8Blocking == 0)
            {
                __enable_irq();
                return KernelError_QueueFull;
            }
            else
            {
                /* Blockieren: in Blocked-State setzen */
                if (Global_PointerToCurrentlyRunningTCB != NULL)
                {
                    Global_PointerToCurrentlyRunningTCB->pWaitingObject = (void*)pQueue;
                    Global_PointerToCurrentlyRunningTCB->eTaskState = TaskState_Blocked;
                    Kernel_RequestContextSwitch();
                }
            }
        }

        __enable_irq();
    }
}

Kernel_ErrorStatus_Enumeration_t Kernel_QueueReceive(Kernel_Queue_t *pQueue, uint32_t *pu32Message, uint8_t u8Blocking)
{
    if (pQueue == NULL || pu32Message == NULL) return KernelError_InvalidParameterProvided;

    while (1)
    {
        __disable_irq();

        if (pQueue->u8Count > 0)
        {
            /* Nachricht vorhanden: aus Ringpuffer lesen */
            *pu32Message = pQueue->au32Buffer[pQueue->u8Tail];
            pQueue->u8Tail = (pQueue->u8Tail + 1) % pQueue->u8Size;
            pQueue->u8Count--;

            /* Eine wartende Task (die auf freien Platz wartet) mit höchster Prio aufwecken */
            TCB_sctTCB_t *pTaskToWake = NULL;
            uint8_t u8HighestWaitingPrio = 255;
            for (uint8_t i = 0; i < Global_TotalNumberOfCreatedTasks; i++)
            {
                if (Global_ArrayOfAllTCBs[i].eTaskState == TaskState_Blocked &&
                    Global_ArrayOfAllTCBs[i].pWaitingObject == (void*)pQueue)
                {
                    if (Global_ArrayOfAllTCBs[i].u8CurrentPriority < u8HighestWaitingPrio)
                    {
                        u8HighestWaitingPrio = Global_ArrayOfAllTCBs[i].u8CurrentPriority;
                        pTaskToWake = &Global_ArrayOfAllTCBs[i];
                    }
                }
            }

            if (pTaskToWake != NULL)
            {
                pTaskToWake->eTaskState = TaskState_Ready;
                pTaskToWake->pWaitingObject = NULL;
                Kernel_RequestContextSwitch();
            }

            __enable_irq();
            return KernelError_NoErrorMessage;
        }
        else
        {
            /* Puffer leer */
            if (u8Blocking == 0)
            {
                __enable_irq();
                return KernelError_QueueEmpty;
            }
            else
            {
                /* Blockieren: in Blocked-State setzen */
                if (Global_PointerToCurrentlyRunningTCB != NULL)
                {
                    Global_PointerToCurrentlyRunningTCB->pWaitingObject = (void*)pQueue;
                    Global_PointerToCurrentlyRunningTCB->eTaskState = TaskState_Blocked;
                    Kernel_RequestContextSwitch();
                }
            }
        }

        __enable_irq();
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