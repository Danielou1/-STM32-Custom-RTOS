# PE5001 - Custom Real-Time Operating System (RTOS)

This project involves the development of a custom RTOS for the STM32 Discovery Kit IoT Node as part of the Master's module "Echtzeitsysteme" (SS26) at THM.

**Developer:** Danielou Mounsande

---

## Project Status & TODO List (V-Model Compliant)

### 1. Project Setup & Infrastructure
- [x] **Git Repository:** Initialize repository and establish project structure.
- [x] **Toolchain Configuration:** Configured `.gitignore` for STM32CubeIDE.
- [x] **Architecture Specification:** Defined FPU (Soft-FP) and Stack (MSP) configuration as per requirements.

### 2. Kernel Implementation (Implementation Phase - Hardware Verification Pending)
- [x] **TCB (Task Control Block):** Implemented data structure (provided by Professor Schmidt).
- [x] **Task Initialization:** Logic for manual stack preparation (simulated Hardware/Software stack frames).
- [x] **Context Switch Core:** ARM assembly implementation in `PendSV_Handler` (R4-R11 saving/restoring).
- [ ] **Scheduling Algorithm:** Round-Robin logic implemented (awaiting hardware validation).
- [ ] **System Tick:** SysTick integration prepared for periodic task switching.

### 3. Verification & Validation (Planned)
- [ ] **Hardware Recognition:** Resolve controller detection issues on the development PC.
- [ ] **Integration Testing:** Verify task switching functionality using LED signaling.
- [ ] **Stack Integrity Check:** Validate correct register restoration during context switches.

---

## Technical Overview (Current Design)

- **Architecture:** ARM Cortex-M4 (STM32L475).
- **Context Switching:** Utilizes the `PendSV` exception for the dispatcher mechanism.
- **Nomenclature:** Transitioned to descriptive, aussagekräftige identifiers (e.g., `Global_PointerToCurrentlyRunningTCB`) to enhance code maintainability and clarity for the upcoming review.
