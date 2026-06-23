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
- [x] **Kernel Objects (Phase 3):** Implemented **Priority Inheritance Protocol (PIP)** and **Message Queues** (fully verified via TeSSLa).

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

---

## Automated Runtime Verification (TeSSLa)

To ensure the kernel's correctness, we use **TeSSLa (Temporal Stream-based Specification Language)** to perform runtime verification of key RTOS properties directly on hardware traces:
1. **Mutual Exclusion (`mutex_error`)**: Ensures that at most one task holds a mutex at any time.
2. **Response Time Constraint (`deadline_violation`)**: Ensures that the high-priority task (Task 1) is scheduled within 15 ms of entering the `Ready` state.
3. **Priority Inversion Prevention (`medium_running_during_inversion`)**: Verifies that the Priority Inheritance Protocol (PIP) operates correctly by checking that a medium-priority task (Task 2) never pre-empts/runs while the high-priority task (Task 1) is blocked on a mutex held by the low-priority task (Task 3).

### Automated Pipeline Components
We have automated the process of compiling, recording traces from the board, filtering the data, and running the verification engine:

- **`clean_trace.py`**: A Python cleaning script that parses raw serial logs. It:
  - Filters out any lines not conforming to the `timestamp: variable = value` TeSSLa format.
  - Detects hardware resets (timestamps reset to near 0) and discards old data to maintain strict timestamp monotonicity.
  - Removes outliers/corrupted lines caused by UART synchronization issues.
- **`verify_live.ps1`**: A PowerShell script that automates the entire loop:
  1. Opens the target COM port (e.g., `COM16`) at `115200` baud.
  2. Resets the target STM32 MCU via DTR/RTS serial signals.
  3. Records the raw serial data for a specified duration (e.g., 25 seconds).
  4. Runs `clean_trace.py` to produce a cleaned, monotonic trace file (`dos_Tessla_filtered.txt`).
  5. Feeds the cleaned trace and `tessla/verification.tessla` into the TeSSLa interpreter (`tessla-assembly-2.1.0.jar`) to run the checks.
- **`verify.bat`**: A quick batch script helper to clean an existing `dos_Tessla.txt` file and run TeSSLa on it.

### How to Run the Automated Live Verification
To execute a live capture and verification run on COM16:
```powershell
.\verify_live.ps1 -PortName COM16 -DurationSeconds 25
```

### Verification Results Summary
During our automated testing, we recorded the target executing a scenario with three tasks (`TaskHigh` - ID 1, `TaskMedium` - ID 2, `TaskLow` - ID 3) sharing a Mutex. 
Running the verification yielded **zero violations** over the entire trace, confirming the kernel's real-time safety properties:
- **`mutex_error = false`**: Mutual exclusion holds perfectly.
- **`deadline_violation = false`**: The high-priority task meets all response time deadlines (< 15 ms).
- **`medium_running_during_inversion = false`**: Priority inheritance behaves correctly—Task 2 never runs while Task 1 is waiting on Task 3, proving that priority inversion is successfully avoided.

