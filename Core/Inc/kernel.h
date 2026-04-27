/*
 * kernel.h
 *
 *  Created on: Apr 27, 2026
 *      Author: Danielou Mounsande
 */

#ifndef DOS_INC_KERNEL_H_
#define DOS_INC_KERNEL_H_

#include "tcb.h"

/// Maximum number of tasks in the system
#define KERNEL_MAXIMUM_NUMBER_OF_TASKS                    ( 5u )

/// Function pointer for task entry point
typedef void (*Kernel_TaskEntryPointFunctionPointer_t)(void);

/// Error codes for kernel functions
typedef enum
{
    KernelError_NoErrorMessage = 0,
    KernelError_TCBArrayIsFull,
    KernelError_InvalidParameterProvided
} Kernel_ErrorStatus_Enumeration_t;

/**
 * @brief Initializes the kernel hardware and internal structures.
 */
void Kernel_InitializeHardwareAndTCBStructures(void);

/**
 * @brief Creates a new task in the system.
 * @param taskFunctionPointer Pointer to the task function.
 * @param taskPriority Priority of the task.
 * @return KernelError_NoErrorMessage if successful.
 */
Kernel_ErrorStatus_Enumeration_t Kernel_CreateNewTask(Kernel_TaskEntryPointFunctionPointer_t taskFunctionPointer, uint8_t taskPriority);

/**
 * @brief Starts the scheduler and begins execution.
 */
void Kernel_StartScheduling(void);

/**
 * @brief Requests a context switch (via PendSV).
 */
void Kernel_RequestContextSwitch(void);

#endif /* DOS_INC_KERNEL_H_ */
