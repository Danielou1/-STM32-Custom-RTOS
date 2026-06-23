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

typedef enum
{
    KernelObjectType_Semaphore = 0,
    KernelObjectType_Mutex,
    KernelObjectType_Queue
} KernelObjectType_t;

typedef struct
{
    KernelObjectType_t eObjectType;
} Kernel_ObjectHeader_t;

/**
 * @brief Struktur für ein Semaphor-Objekt.
 */
typedef struct
{
    KernelObjectType_t eObjectType; /* Muss das erste Element sein! */
    uint32_t u32Count;         /* Aktuelle Anzahl der verfügbaren Jetons */
    uint32_t u32MaxCount;      /* Maximale Kapazität (verhindert Logikfehler) */
} Kernel_Semaphore_t;

/**
 * @brief Struktur für ein Mutex-Objekt.
 */
typedef struct
{
    KernelObjectType_t eObjectType; /* Muss das erste Element sein! */
    TCB_sctTCB_t *pOwner;      /* Zeiger auf die Task, die den Mutex aktuell hält */
    uint8_t u8IsLocked;        /* Sperrzustand: 1 = gesperrt, 0 = frei */
} Kernel_Mutex_t;

#define KERNEL_QUEUE_MAX_SIZE                             ( 16u )

/**
 * @brief Struktur für ein Message-Queue-Objekt.
 */
typedef struct
{
    KernelObjectType_t eObjectType;                /* Muss das erste Element sein! */
    uint32_t au32Buffer[KERNEL_QUEUE_MAX_SIZE];    /* Ringpuffer für Nachrichten (Werte/Pointer) */
    uint8_t u8Head;                                /* Schreib-Index */
    uint8_t u8Tail;                                /* Lese-Index */
    uint8_t u8Count;                               /* Aktuelle Anzahl der Nachrichten im Puffer */
    uint8_t u8Size;                                /* Maximale Kapazität des Ringpuffers */
} Kernel_Queue_t;

/// Fehlercodes für Kernel-Funktionen
typedef enum
{
    KernelError_NoErrorMessage = 0,
    KernelError_TCBArrayIsFull,
    KernelError_InvalidParameterProvided,
    KernelError_TaskNotFound,
    KernelError_QueueEmpty,
    KernelError_QueueFull
} Kernel_ErrorStatus_Enumeration_t;

/**
 * @brief Initialisiert die Kernel-Hardware und interne Strukturen.
 */
void Kernel_InitializeHardwareAndTCBStructures(void);

/**
 * @brief Erstellt einen neuen Task im System.
 * @param taskFunctionPointer Zeiger auf die Task-Funktion.
 * @param u8Priority Priorität der Task (0 = höchste, 255 = niedrigste).
 * @return KernelError_NoErrorMessage bei Erfolg.
 */
Kernel_ErrorStatus_Enumeration_t Kernel_CreateNewTask(Kernel_TaskEntryPointFunctionPointer_t taskFunctionPointer, uint8_t u8Priority);

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

/**
 * @brief Initialisiert ein Mutex.
 * @param pMutex Zeiger auf das Mutex-Objekt.
 */
void Kernel_MutexInit(Kernel_Mutex_t *pMutex);

/**
 * @brief Sperrt ein Mutex (Lock/Pend). 
 * Wenn besetzt, wird die Task blockiert.
 */
void Kernel_MutexLock(Kernel_Mutex_t *pMutex);

/**
 * @brief Entsperrt ein Mutex (Unlock/Post).
 * Nur der Besitzer darf das Mutex freigeben.
 */
void Kernel_MutexUnlock(Kernel_Mutex_t *pMutex);

/**
 * @brief Initialisiert eine Message Queue.
 * @param pQueue Zeiger auf das Queue-Objekt.
 * @param u8Size Maximale Größe der Queue (darf KERNEL_QUEUE_MAX_SIZE nicht überschreiten).
 */
void Kernel_QueueInit(Kernel_Queue_t *pQueue, uint8_t u8Size);

/**
 * @brief Sendet eine Nachricht an die Queue.
 * @param pQueue Zeiger auf das Queue-Objekt.
 * @param u32Message Die Nachricht (Wert oder Zeiger).
 * @param u8Blocking 1 = Blockierend (wenn voll), 0 = Nicht-Blockierend.
 * @return KernelError_NoErrorMessage bei Erfolg, oder Fehlercode.
 */
Kernel_ErrorStatus_Enumeration_t Kernel_QueueSend(Kernel_Queue_t *pQueue, uint32_t u32Message, uint8_t u8Blocking);

/**
 * @brief Liest eine Nachricht aus der Queue.
 * @param pQueue Zeiger auf das Queue-Objekt.
 * @param pu32Message Zeiger auf die Speicheradresse für das Ergebnis.
 * @param u8Blocking 1 = Blockierend (wenn leer), 0 = Nicht-Blockierend.
 * @return KernelError_NoErrorMessage bei Erfolg, oder Fehlercode.
 */
Kernel_ErrorStatus_Enumeration_t Kernel_QueueReceive(Kernel_Queue_t *pQueue, uint32_t *pu32Message, uint8_t u8Blocking);

/**
 * @brief Prüft, ob die Queue leer ist.
 * @param pQueue Zeiger auf das Queue-Objekt.
 * @return 1 wenn leer, 0 wenn nicht leer.
 */
uint8_t Kernel_QueueIsEmpty(Kernel_Queue_t *pQueue);

/**
 * @brief Prüft, ob die Queue voll ist.
 * @param pQueue Zeiger auf das Queue-Objekt.
 * @return 1 wenn voll, 0 wenn nicht voll.
 */
uint8_t Kernel_QueueIsFull(Kernel_Queue_t *pQueue);

/* Globaler Zeiger auf den aktuell laufenden Task - Für Threadsicherheit als volatile markiert */
extern TCB_sctTCB_t * volatile Global_PointerToCurrentlyRunningTCB;

#endif /* DOS_INC_KERNEL_H_ */