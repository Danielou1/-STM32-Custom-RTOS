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

/**
 * @brief Struktur für ein Semaphor-Objekt.
 */
typedef struct
{
    uint32_t u32Count;         /* Aktuelle Anzahl der verfügbaren Jetons */
    uint32_t u32MaxCount;      /* Maximale Kapazität (verhindert Logikfehler) */
} Kernel_Semaphore_t;

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
 * @brief Versetzt die aktuelle Task für eine bestimmte Zeit in den Blocked-State.
 * @param u32DelayInTicks Anzahl der System-Ticks (ms) zum Warten.
 */
void Kernel_TaskDelay(uint32_t u32DelayInTicks);

/**
 * @brief Aktualisiert die Wartezeiten aller blockierten Tasks. 
 * Sollte im SysTick-Handler aufgerufen werden.
 */
void Kernel_UpdateTimers(void);

/**
 * @brief Startet den Scheduler und beginnt die Ausführung.
 */
void Kernel_StartScheduling(void);

/**
 * @brief Fordert einen Kontextwechsel an (via PendSV).
 */
void Kernel_RequestContextSwitch(void);

/**
 * @brief Initialisiert ein Semaphor.
 * @param pSemaphore Zeiger auf das Semaphor-Objekt.
 * @param u32InitialCount Startwert der Jetons.
 * @param u32MaxCount Maximalwert der Jetons.
 */
void Kernel_SemaphoreInit(Kernel_Semaphore_t *pSemaphore, uint32_t u32InitialCount, uint32_t u32MaxCount);

/**
 * @brief Versucht, einen Jeton vom Semaphor zu nehmen (Wait/Pend).
 * @param pSemaphore Zeiger auf das Semaphor-Objekt.
 */
void Kernel_SemaphoreWait(Kernel_Semaphore_t *pSemaphore);

/**
 * @brief Gibt einen Jeton an das Semaphor zurück (Signal/Post).
 * @param pSemaphore Zeiger auf das Semaphor-Objekt.
 */
void Kernel_SemaphoreGive(Kernel_Semaphore_t *pSemaphore);

/* Globaler Zeiger auf den aktuell laufenden Task - Für Threadsicherheit als volatile markiert */
extern TCB_sctTCB_t * volatile Global_PointerToCurrentlyRunningTCB;

#endif /* DOS_INC_KERNEL_H_ */