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
- [ ] **Kernel Objects:** (Upcoming) Semaphores and Message Queues implementation.

---

## Technical Architecture & Design Strategies

### 1. Hybrid Context Switch (R0-Delegation)
To eliminate pointer corruption and literal pool risks in `naked` assembly functions, the system delegates stack pointer calculation to a C function (`Kernel_ContextSwitch`). 
- **Assembly:** Handles low-level register saving/restoring (R4-R11).
- **C-Logic:** Manages TCB structures and returns the next stack pointer via register `R0`.

### 2. Boot-Lock Mechanism
A global security flag `Global_OsIsRunning` prevents the `SysTick` from triggering a context switch during the critical hardware initialization phase (`HAL_Init`), avoiding crashes on uninitialized tasks.

### 3. AAPCS Compliance
- **8-Byte Alignment:** Stack pointers are strictly aligned to 8-byte boundaries.
- **Register Vaulting:** The `EXC_RETURN` code is preserved in `R4` (callee-saved) during C-function calls within interrupts.

### 4. Status Signaling
- **LD1 (PA5):** Toggled by Task 1.
- **LD2 (PB14):** Toggled by Task 2.
- **LD3 (PC9):** Toggled at every context switch (Kernel heartbeat).

---

## Documentation
Detailed forensic analysis and project logs are maintained in `RTOS_Dokumentation_Detail_2026-04-27.tex`.
