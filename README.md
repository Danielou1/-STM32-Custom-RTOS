# PE5001 - Custom Real-Time Operating System (RTOS)

This project involves the development of a custom RTOS for the STM32 Discovery Kit IoT Node as part of the Master's module "Echtzeitsysteme" (SS26) at THM.

**Developer:** Danielou Mounsande

---

## Project Status & TODO List (V-Model Compliant)

### 1. Project Setup & Infrastructure
- [x] **Git Repository:** Initialize repository and establish project structure.
- [x] **Toolchain Configuration:** Configured `.gitignore` for STM32CubeIDE.
- [x] **Architecture Specification:** Defined FPU (Soft-FP) and Stack (MSP) configuration.

### 2. Kernel Core (Stabilized Phase)
- [x] **TCB (Task Control Block):** Implemented descriptive data structures.
- [x] **Task Initialization:** Manual stack preparation with mandatory **Thumb Bit (LSB=1)** forcing.
- [x] **Context Switch Core:** Refactored into a **Hybrid C/Assembly architecture** for maximum stability.
- [x] **Scheduling Algorithm:** Preemptive Round-Robin logic fully verified on hardware.
- [x] **System Tick:** Integrated SysTick as the heartbeat for preemptive switching.

### 3. Verification & Validation (Success)
- [x] **Hardware Recognition:** J-Link/ST-Link connectivity stabilized.
- [x] **Integration Testing:** Context switching verified via **LED signaling (LD1/PA5 and LD2/PB14)**.
- [x] **Forensic Analysis:** Successfully debugged INVPC (UsageFault) and Null-Pointer corruption issues.
- [x] **Kernel Objects (Phase 1):** Implemented Counting Semaphores and TCB-based blocking logic.
- [x] **Kernel Objects (Phase 2):** Implemented Mutexes with **Ownership-check** and switched to **Priority-based Scheduling**.
- [ ] **Kernel Objects (Phase 3):** (Upcoming) Priority Inheritance and Message Queues.

---

## Technical Architecture & Design Strategies

### 1. Hybrid Context Switch (R0-Delegation)
To eliminate pointer corruption and literal pool risks in `naked` assembly functions, the system delegates stack pointer calculation to a C function (`Kernel_ContextSwitch`). 
- **Assembly:** Handles low-level register saving/restoring (R4-R11).
- **C-Logic:** Manages TCB structures and returns the next stack pointer via register `R0`.

### 2. Priority-Based Scheduling
The system has moved beyond simple Round-Robin. The scheduler now implements a **minimum-search algorithm** over all ready tasks:
- **Ranking:** Lower numerical values represent higher urgency (Prio 0 = highest).
- **Idle Task:** Fixed at Prio 255 to ensure background execution when no user tasks are ready.
- **Dynamic Priorities:** The TCB structure supports both `BasePriority` and `CurrentPriority` to enable future Priority Inheritance.

### 3. Mutexes & Ownership
Unlike semaphores, DOS Mutexes implement a strict ownership policy:
- **Security:** Only the task that locked a mutex is authorized to unlock it.
- **Internal Logic:** The kernel tracks the `pOwner` in the mutex structure and verifies it against the current TCB during release operations.

### 4. Semaphor & Blocking Logic
The kernel supports an object-based blocking mechanism. When a task calls `Kernel_SemaphoreWait` or `Kernel_MutexLock` and no resources are available:
- The task's `pWaitingObject` pointer is set to the object's address.
- The task state is changed to `TaskState_Blocked`.
- Release functions (`Give`/`Unlock`) perform a targeted search over all TCBs to wake up exactly the task waiting for that specific object.

### 5. Boot-Lock Mechanism
A global security flag `Global_OsIsRunning` prevents the `SysTick` from triggering a context switch during the critical hardware initialization phase (`HAL_Init`), avoiding crashes on uninitialized tasks.

### 6. AAPCS Compliance
- **8-Byte Alignment:** Stack pointers are strictly aligned to 8-byte boundaries.
- **Register Vaulting:** AAPCS rules are strictly followed during C calls within the PendSV interrupt.

### 7. Status Signaling
- **LD1 (PA5):** Toggled by Task 1 (used for Semaphore/Mutex test scenarios).
- **LD3 (PC9):** Toggled at every context switch (Kernel heartbeat).

---

## Documentation
Detaillierte Definitionen, Funktionszyklen und technische Analysen sind in `RTOS_Dokumentation.tex` (Deutsch) dokumentiert.
