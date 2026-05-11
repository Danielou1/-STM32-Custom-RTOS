/*
 * kernel.h
 *
 * Erstellt am: 27. Apr 2026
 * Autor: Danielou Mounsande
 */

#ifndef DOS_INC_KERNEL_H_
#define DOS_INC_KERNEL_H_

#include "tcb.h"

/// Maximale Anzahl von Tasks im System
#define KERNEL_MAXIMUM_NUMBER_OF_TASKS                    ( 6u )

/// IDLE Task Priorität (niedrigste) - Kann belassen oder entfernt werden, wird nicht mehr verwendet
#define KERNEL_IDLE_TASK_PRIORITY                         ( 255u )

/// Funktionszeiger für den Task-Einstiegspunkt
typedef void (*Kernel_TaskEntryPointFunctionPointer_t)(void);

/// Fehlercodes für Kernel-Funktionen
typedef enum
{
    KernelError_NoErrorMessage = 0,
    KernelError_TCBArrayIsFull,
    KernelError_InvalidParameterProvided,
    KernelError_TaskNotFound
} Kernel_ErrorStatus_Enumeration_t;

/**
 * @brief Initialisiert die Kernel-Hardware und interne Strukturen.
 */
void Kernel_InitializeHardwareAndTCBStructures(void);

/**
 * @brief Erstellt einen neuen Task im System.
 * @param taskFunctionPointer Zeiger auf die Task-Funktion.
 * @return KernelError_NoErrorMessage bei Erfolg.
 */
Kernel_ErrorStatus_Enumeration_t Kernel_CreateNewTask(Kernel_TaskEntryPointFunctionPointer_t taskFunctionPointer);

/**
 * @brief Startet den Scheduler und beginnt die Ausführung.
 */
void Kernel_StartScheduling(void);

/**
 * @brief Fordert einen Kontextwechsel an (via PendSV).
 */
void Kernel_RequestContextSwitch(void);

/* Globaler Zeiger auf den aktuell laufenden Task - Für Threadsicherheit als volatile markiert */
extern TCB_sctTCB_t * volatile Global_PointerToCurrentlyRunningTCB;

#endif /* DOS_INC_KERNEL_H_ */