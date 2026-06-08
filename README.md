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
- [ ] **Kernel Objects (Phase 2):** (Upcoming) Mutexes with Priority Inheritance and Message Queues.

---

## Technical Architecture & Design Strategies

### 1. Hybrid Context Switch (R0-Delegation)
To eliminate pointer corruption and literal pool risks in `naked` assembly functions, the system delegates stack pointer calculation to a C function (`Kernel_ContextSwitch`). 
- **Assembly:** Handles low-level register saving/restoring (R4-R11).
- **C-Logic:** Manages TCB structures and returns the next stack pointer via register `R0`.

### 2. Semaphor & Blocking Logic
The kernel now supports an object-based blocking mechanism. When a task calls `Kernel_SemaphoreWait` and no tokens are available:
- The task's `pWaitingObject` pointer is set to the semaphore's address.
- The task state is changed to `TaskState_Blocked`.
- `Kernel_SemaphoreGive` performs a targeted search over all TCBs to wake up exactly the task waiting for that specific object.

### 3. Boot-Lock Mechanism
A global security flag `Global_OsIsRunning` prevents the `SysTick` from triggering a context switch during the critical hardware initialization phase (`HAL_Init`), avoiding crashes on uninitialized tasks.

### 4. AAPCS Compliance
- **8-Byte Alignment:** Stack pointers are strictly aligned to 8-byte boundaries.
- **Register Vaulting:** AAPCS rules are strictly followed during C calls within the PendSV interrupt.

### 5. Status Signaling
- **LD1 (PA5):** Toggled by Task 1 (and used for Semaphore test scenario).
- **LD3 (PC9):** Toggled at every context switch (Kernel heartbeat).

---

## Documentation
Detaillierte Definitionen, Funktionszyklen und technische Analysen sind in `RTOS_Dokumentation.tex` (Deutsch) dokumentiert.
