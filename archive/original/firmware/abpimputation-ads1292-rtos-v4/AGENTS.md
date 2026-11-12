# PhysioCard — Agent Guide

STM32F103C8 (Cortex-M3) embedded firmware for a physiological monitoring device. C99, FreeRTOS V9.0.0, STM32F10x StdPeriph V3.5.0. No CI, no tests, no README.

## Build systems (both AC5 ARMCC v5.06)

- **Keil uVision** — `Project.uvprojx` (primary, opened via IDE)
- **EIDE (VS Code)** — `.eide/eide.yml`, driven by `cl.eide` extension

VS Code tasks (EIDE): `build`, `flash`, `rebuild`, `clean`. No CLI-only build command exists.

## Formatting

```sh
clang-format -i <file>.c <file>.h
```

Config: `.clang-format` (Microsoft style, 4-space indent, Linux braces).

## Active vs dead code

| File | Status |
|------|--------|
| `Tasks/TEMP.c` / `MAX30205.c` | **Active** — only running task |
| `User/filters.c/.h` | **Fully commented out** (dead code) |
| `Tasks/usart.c` | Active (serial + printf via USART1) |
| `Tasks/LED.c` | Active (GPIO init), but LED tasks commented out |
| `User/tasks_config.c` | Only `TEMP_Task` is created; all other `xTaskCreate` calls commented |
| `Tasks/rtos_comm.h` | Empty file |
| `User/stm32f10x_it.c` | FreeRTOS handlers (`SVC_Handler`, `PendSV_Handler`, `SysTick_Handler`) commented out — they are mapped via `#define` in `FreeRTOSConfig.h` to FreeRTOS port layer symbols |

## Hardware constraints

- **SRAM:** 20 KB total; `configTOTAL_HEAP_SIZE = 17 KB` (heap_4.c), leaving ~3 KB
- **I2C1:** PC5 (SCL), PC4 (SDA), 50 kHz, MAX30205 at 7-bit address `0x48`
- **USART1:** PA9 (TX), PA10 (RX), 115200 baud — `printf()` redirects here
- **System clock:** HSE 8 MHz × 9 PLL = 72 MHz

## Code conventions

- C99, no C++
- 4-space indent, no tabs, Linux brace placement
- Doxygen-style `/** */` for APIs, Chinese section headers (头文件包含区, 私有函数实现区, etc.)
- Hungarian-like naming: `g_` prefix for globals, snake_case for functions
- Copyright header: `EUIN_模电课设 / PESC Project`, 2026

## Gotchas

- I2C driver (`MAX30205.c`) has GPIO bit-bang bus recovery, retry logic (3 attempts on init)
- `#pragma import(__use_no_semihosting)` + `_sys_exit()` stub in `usart.c` for ARMCC compatibility
- No package manager; all deps vendored (FreeRTOS, StdPeriph, CMSIS, DFP)
- `.gitignore` does NOT exclude `Objects/` (uVision artifacts would be tracked)
