# Current boot flow

The only executable flow present is the vendor startup flow, not a complete bootloader:

1. `Reset_Handler` clears SRAM controller flags.
2. It copies `.data` and retention data, clears `.bss` and retention BSS, configures SRAM wait state registers, then calls `__libc_init_array`.
3. It calls `SystemInit`.
4. Existing `SystemInit` enables the FPU, calculates `SystemCoreClock`, writes VTOR=`0x8000`, and enables interrupts.
5. It calls `main`, but no `main` exists in the checked-out sources.

Reset-source capture, software-reset detection, reset-flag clearing, boot timeout, application-valid flag, parameter address, vector validation, application jump, interrupt/SysTick cleanup, watchdog, and external watchdog behavior are all `UNKNOWN` because their legacy sources are absent.

