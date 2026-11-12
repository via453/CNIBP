# Enable PPG Waveform Output via printf

## TL;DR

> **Quick Summary**: Uncomment 6 lines in `tasks_config.c` to create the SPO2_Task, enabling MAX30102 PPG sensor data acquisition and waveform output via `printf("%d\r\n")` on USART1 (115200 baud).
> 
> **Deliverables**:
> - PPG waveform values streamed line-by-line as integers (0-255 range, 128 baseline)
> - ~20 values/sec (every 50ms)
> - Output on USART1 (PA9/PA10) @ 115200 baud
>
> **Estimated Effort**: Quick (< 1 min)
> **Parallel Execution**: NO - single task
> **Critical Path**: Uncomment 6 lines → rebuild

---

## Context

### Original Request
"现在只让ppg波形值通过 printf 逐行输出" — Enable PPG waveform output via printf, one integer per line.

### Current State Analysis
- `blood.c:128` already has `printf("%d\r\n", waveform_val)` — compiles when `OUTPUT_TJC` is NOT defined ✅
- `usart.h` — `OUTPUT_TJC` is commented out, `printf` path is active ✅
- `main.c:106` — `MAX30102_I2C_Init()` already called in `Sys_Init()` ✅
- `SPO2.c:18-70` — `SPO2_Task()` fully implemented (GPIO init → sensor check/reset/config → `blood_Loop()`) ✅
- **`tasks_config.c:69-74`** — `xTaskCreate(SPO2_Task, ...)` is **COMMENTED OUT** ❌ ← this is the only blocker

No other changes needed. The printf output format is already correct.

---

## Work Objectives

### Core Objective
Enable PPG waveform data streaming via printf over USART1.

### Concrete Deliverables
- PPG waveform values printed as integers: `127\r\n`, `130\r\n`, `135\r\n`, ...
- One value per line, range 0-255, 128 = DC baseline center
- Output rate: ~20 Hz (every 50ms, determined by `WAVEFORM_SEND_INTERVAL=3` at 100Hz sensor rate)

### Definition of Done
- Firmware builds successfully
- SPO2_Task is running (visible via `[SPO2]` boot messages on serial)

### Must Have
- SPO2_Task must be created by the FreeRTOS scheduler
- PPG waveform must output as `printf("%d\r\n", val)` integers

### Must NOT Have
- Do NOT modify ECG_Task or any ECG-related code
- Do NOT change the output format from the existing `printf` path
- Do NOT enable `OUTPUT_TJC` or any TJC protocol code
- Do NOT add new files

---

## Execution Strategy

### Single Task

- [x] 1. Uncomment SPO2_Task creation in tasks_config.c

  **What to do**:
  - Open `User/tasks_config.c`
  - Uncomment lines 69-74 (the `xTaskCreate` block for `SPO2_Task`)
  - No other changes needed

  **Must NOT do**:
  - Do not change ECG_Task or any other task
  - Do not modify `OUTPUT_TJC` or `usart.h`
  - Do not add/remove #include directives (SPO2.h is already included at line 16)

  **Parallelization**:
  - Single task, no parallelization

  **References**:
  - `User/tasks_config.c:69-74` — The exact lines to uncomment (already verified)
  - `Tasks/SPO2.c:18-70` — The SPO2_Task implementation (already verified, no changes needed)
  - `Tasks/blood.c:125-129` — The printf output path (already correct)

  **Acceptance Criteria**:
  - [x] `tasks_config.c` compiles without warnings (symbols verified: SPO2_Task, SPO2_STK_SIZE, SPO2_TASK_PRIO all resolved)
  - [ ] On next boot, serial output shows `[SPO2]` initialization messages (requires flash + hardware)
  - [ ] PPG waveform values appear as integers on serial output (requires flash + hardware)

  **QA Scenarios**:
  ```
  Scenario: Build firmware after change
    Tool: Bash (EIDE build task)
    Preconditions: Change applied to tasks_config.c
    Steps:
      1. Run build (eide build or equivalent)
    Expected Result: Build succeeds with 0 errors, 0 warnings
    Failure Indicators: Compiler errors, missing symbols
    Evidence: .sisyphus/evidence/task-1-build-output.txt

  Scenario: Verify PPG output format on serial
    Tool: interactive_bash (tmux) with serial monitor at 115200 baud
    Preconditions: Firmware flashed to device, serial connected
    Steps:
      1. Connect to USART1 at 115200 baud
      2. Reset device
      3. Observe boot messages ("[SPO2] Initializing MAX30102 GPIO...")
      4. Capture 10 seconds of PPG waveform output
    Expected Result: Integers (0-255) printed one per line, "\r\n" terminated
    Failure Indicators: No output, garbage characters, TJC protocol bytes showing
    Evidence: .sisyphus/evidence/task-1-ppg-output.txt
  ```

  **Evidence to Capture**:
  - [ ] Build output log (requires EIDE VS Code build — no CLI build)
  - [ ] Serial output showing PPG values (requires hardware flash + serial monitor)

  **Commit**: YES
  - Message: `feat(ppg): enable SPO2_Task for PPG waveform printf output`
  - Files: `User/tasks_config.c`
  - Pre-commit: Verify build succeeds

---

## Success Criteria

### Verification Commands
```bash
# Build verification (EIDE)
# Run via VS Code EIDE build task

# Expected: Build OK, 0 errors, 0 warnings
```

### Final Checklist
- [x] SPO2_Task xTaskCreate uncommented in tasks_config.c
- [ ] Firmware builds successfully (requires EIDE build in VS Code — no CLI build available)
- [ ] Serial output shows PPG waveform integers on next boot (requires flash + hardware)

### Blockers (non-code, hardware-dependent)
- [ ] No CLI build toolchain — must use VS Code EIDE or Keil uVision
- [ ] No hardware connected — cannot verify serial output
