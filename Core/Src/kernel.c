/*
 * kernel.c
 *
 * Created on: Apr 27, 2026
 * Author: Danielou Mounsande
 */

#include "kernel.h"
#include <string.h>
#include "main.h"
#include "tcb.h"

/* --- Section 1: Définitions Matérielles (Cortex-M4) --- */

#define INTERRUPT_CONTROL_STATE_REGISTER_ADDRESS                             ( 0xE000ED04u )
#define PENDSV_SET_BIT_MASK                                                  ( 1u << 28 )
#define SYSTEM_HANDLER_PRIORITY_REGISTER_3_ADDRESS                           ( 0xE000ED22u )
#define PENDSV_PRIORITY_LOWEST                                               ( 0xFFu )
#define COPROCESSOR_ACCESS_CONTROL_REGISTER_ADDRESS                          ( 0xE000ED88u )
#define CORTEX_M4_INITIAL_xPSR_VALUE                                         ( 1u << 24 )

/* --- Section 2: État Interne du Kernel --- */

static TCB_sctTCB_t Global_ArrayOfAllTCBs[KERNEL_MAXIMUM_NUMBER_OF_TASKS] __attribute__((aligned(8)));
static uint8_t Global_TotalNumberOfCreatedTasks = 0;
static uint8_t Global_IndexDerAktuellenTask = 0;
static uint8_t Global_OsIsRunning = 0; /* Verrou de sécurité au démarrage */

TCB_sctTCB_t * volatile Global_PointerToCurrentlyRunningTCB = NULL;


/* --- Section 3: Fonctions Internes et Hooks --- */

/* Filet de sécurité : Si une tâche tente de faire un "return" ou se termine,
 * elle atterrira ici au lieu de provoquer un HardFault à l'adresse 0x00000000.
 */
static void Kernel_TaskReturnHook(void)
{
    while(1)
    {
        /* Si le débogueur s'arrête ici, c'est qu'une de tes tâches n'a pas de boucle while(1) ! */
        __asm volatile ("nop");
    }
}

/* Tâche par défaut pour occuper le CPU si rien d'autre ne tourne */
static void Kernel_IdleTask(void)
{
    while(1)
    {
        __asm volatile ("nop");
    }
}

/* Ordonnanceur intelligent : prend l'ancien pointeur de pile, retourne le nouveau ! */
uint32_t Kernel_ContextSwitch(uint32_t current_sp)
{
    /* DEBUG : Faire clignoter la LED3 (PC9) à chaque changement de contexte pour prouver que le Kernel vit */
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_9);

    /* 1. Sauvegarde le SP de la tâche sortante */
    if (Global_PointerToCurrentlyRunningTCB != NULL)
    {
        Global_PointerToCurrentlyRunningTCB->u32TaskSP = current_sp;
    }

    /* 2. Sélection de la tâche suivante (Round Robin) */
    Global_IndexDerAktuellenTask++;
    if (Global_IndexDerAktuellenTask >= Global_TotalNumberOfCreatedTasks)
    {
        Global_IndexDerAktuellenTask = 0;
    }

    /* 3. Charge la nouvelle tâche */
    Global_PointerToCurrentlyRunningTCB = &Global_ArrayOfAllTCBs[Global_IndexDerAktuellenTask];
    Global_PointerToCurrentlyRunningTCB->eTaskState = TaskState_Running;

    /* 4. Retourne le pointeur de pile exact */
    return Global_PointerToCurrentlyRunningTCB->u32TaskSP;
}

/* --- Section 3: Fonctions de l'API du Kernel --- */

void Kernel_InitializeHardwareAndTCBStructures(void)
{
    /* Désactiver la FPU (coprocesseur mathématique) pour simplifier le contexte */
    volatile uint32_t *cpacr = (volatile uint32_t *)COPROCESSOR_ACCESS_CONTROL_REGISTER_ADDRESS;
    *cpacr &= ~(0xFu << 20);

    /* Mettre la priorité de PendSV au plus bas possible */
    volatile uint8_t *shpr3 = (volatile uint8_t *)SYSTEM_HANDLER_PRIORITY_REGISTER_3_ADDRESS;
    *shpr3 = PENDSV_PRIORITY_LOWEST;

    memset(Global_ArrayOfAllTCBs, 0, sizeof(Global_ArrayOfAllTCBs));
    Global_TotalNumberOfCreatedTasks = 0;
    Global_PointerToCurrentlyRunningTCB = NULL;

    /* Création automatique de la tâche Idle en position 0 */
    Kernel_CreateNewTask(Kernel_IdleTask);
}

Kernel_ErrorStatus_Enumeration_t Kernel_CreateNewTask(Kernel_TaskEntryPointFunctionPointer_t taskFunctionPointer)
{
    if (Global_TotalNumberOfCreatedTasks >= KERNEL_MAXIMUM_NUMBER_OF_TASKS) return KernelError_TCBArrayIsFull;
    if (taskFunctionPointer == NULL) return KernelError_InvalidParameterProvided;

    TCB_sctTCB_t *pTCB = &Global_ArrayOfAllTCBs[Global_TotalNumberOfCreatedTasks];
    pTCB->eTaskState = TaskState_Ready;

    /* 1. Placement du pointeur de pile à la FIN du tableau (Full Descending) */
    uint32_t *sp = &pTCB->au32TaskStack[TCB_TASK_STACK_SIZE];

    /* 2. Alignement strict sur 8 octets (exigence matérielle ARM) */
    sp = (uint32_t *)(((uint32_t)sp) & 0xFFFFFFF8u);

    /* 3. Faux cadre de pile (Dummy Stack Frame) */
    *(--sp) = CORTEX_M4_INITIAL_xPSR_VALUE;                  /* xPSR (Bit 24 à 1 impératif) */
    *(--sp) = (uint32_t)taskFunctionPointer | 0x01u;         /* PC (Program Counter) + Bit Thumb */
    *(--sp) = (uint32_t)Kernel_TaskReturnHook | 0x01u;       /* LR (Link Register) + Bit Thumb */
    *(--sp) = 0x12121212u;                                   /* R12 */
    *(--sp) = 0x03030303u;                                   /* R3 */
    *(--sp) = 0x02020202u;                                   /* R2 */
    *(--sp) = 0x01010101u;                                   /* R1 */
    *(--sp) = 0x00000000u;                                   /* R0 (Paramètre de la fonction) */

    /* 4. Sauvegarde des registres restants (R4 à R11) */
    *(--sp) = 0x11111111u; /* R11 */
    *(--sp) = 0x10101010u; /* R10 */
    *(--sp) = 0x09090909u; /* R9 */
    *(--sp) = 0x08080808u; /* R8 */
    *(--sp) = 0x07070707u; /* R7 */
    *(--sp) = 0x06060606u; /* R6 */
    *(--sp) = 0x05050505u; /* R5 */
    *(--sp) = 0x04040404u; /* R4 */

    /* 5. On sauvegarde la position actuelle du pointeur dans le TCB */
    pTCB->u32TaskSP = (uint32_t)sp;

    Global_TotalNumberOfCreatedTasks++;

    return KernelError_NoErrorMessage;
}

void Kernel_StartScheduling(void)
{
    if (Global_TotalNumberOfCreatedTasks > 0)
    {
        Global_OsIsRunning = 1;        /* DÉVERROUILLAGE : Le système est prêt ! */
        Global_PointerToCurrentlyRunningTCB = NULL;
        __enable_irq();                /* On s'assure que les interruptions sont activées */
        Kernel_RequestContextSwitch(); /* Déclenche le premier changement de contexte */
        while(1);                      /* On ne doit jamais sortir d'ici */
    }
}

void Kernel_RequestContextSwitch(void)
{
    /* On ignore la demande si l'OS n'est pas encore lancé (sécurité SysTick) */
    if (Global_OsIsRunning == 1)
    {
        volatile uint32_t *icsr = (volatile uint32_t *)INTERRUPT_CONTROL_STATE_REGISTER_ADDRESS;
        *icsr = PENDSV_SET_BIT_MASK;
    }
}

__attribute__((naked)) void PendSV_Handler(void)
{
    __asm volatile (
        "cpsid i\n"              /* Désactive les interruptions */

        "mrs r0, msp\n"          /* R0 = MSP actuel (Sera le 1er argument pour la fonction C !) */

        /* Vérifie si on est au premier démarrage */
        "ldr r1, =Global_PointerToCurrentlyRunningTCB\n"
        "ldr r2, [r1]\n"
        "cbz r2, SKIP_SAVE\n"

        /* Sauvegarde du contexte logiciel */
        "stmdb r0!, {r4-r11}\n"  /* Empile R4-R11 (R0 est mis à jour) */
        "msr msp, r0\n"          /* VERROUILLAGE de la pile en mémoire */

        "SKIP_SAVE:\n"
        /* Appel C avec respect strict de l'architecture ARM (AAPCS) */
        "push {r3, lr}\n"        /* Alignement 8 octets + Sauvegarde du code de retour */
        "bl Kernel_ContextSwitch\n" /* Appelle le C. R0 contient 'current_sp'. Le C retourne 'new_sp' dans R0 ! */
        "pop {r3, lr}\n"         /* Restaure le code de retour */

        /* Restauration du nouveau contexte */
        "ldmia r0!, {r4-r11}\n"  /* R0 contient la nouvelle pile. On dépile R4-R11. */
        "msr msp, r0\n"          /* Met à jour le pointeur matériel ! */
        "isb\n"                  /* Vide le pipeline */

        "cpsie i\n"              /* Réactive les interruptions */
        "bx lr\n"                /* Retourne dans la tâche avec succès ! */
        ".align 4\n"
    );
}
