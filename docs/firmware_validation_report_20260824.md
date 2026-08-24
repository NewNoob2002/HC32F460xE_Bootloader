# HC32F460 bootloader ICG validation report — 2026-08-24

## Result

**Current board passed; old-board power-cycle regression remains pending.**

The startup failure was traced to the firmware not emitting an explicit ICG block at `0x00000400`. The fixed image now disables WDT/SWDT automatic startup, and the linker rejects images with a missing or malformed ICG section.

## Changes

- Build `Drivers/hc32f4xx_ll_drivers/src/hc32_ll_icg.c`.
- Enable `LL_ICG_ENABLE`.
- Assert that `.icg_sec` starts at `0x00000400` and is exactly 32 bytes.
- Make the SRAM flash loader derive its erase span from `IMAGE_LENGTH`.
- Feed an already-running hardware WDT while the SRAM loader waits for flash operations.

## Build and image checks

- Debug, Release and ReleaseNoLog builds: passed.
- Host regression: 2/2 tests passed.
- Debug size: text 27,204 B, data 248 B, BSS 13,884 B.
- Debug BIN: 27,872 bytes.
- ELF SHA-256: `c0a22146a6ee792e3977fd818d3f58923d490e229713f2526fefe6778dce785f`.
- BIN SHA-256: `b693e33aa9385b4f7761ea192010e758740a4f300e71f83447bf1920dda02c15`.
- `.icg_sec`: address `0x00000400`, size `0x20`.
- ICG words: `FFDFFFBF FFFFFEFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF`.
- WDT and SWDT `AUTS` settings: stopped after reset.

## Current-board hardware validation

- Target: HC32F460PETB, J-Link S/N 63728710, VTref 3.300 V.
- SRAM loader erase/program: passed.
- Fresh-session full BIN verification: passed.
- Readback at `0x00000400`: matched the ELF ICG block.
- Software reset and approximately 7 seconds of execution: passed in Thread mode; CFSR/HFSR were zero.
- Manual power-cycle startup: reached the bootloader main loop; WDT/SWDT status remained zero.
- Clean 100 kHz SWD sample after approximately 19.8 seconds: Thread mode, CFSR/HFSR zero, CPACR `0x00F00000`; target resumed afterward.

At 4 MHz, this board showed corrupted SWD reads and a debugger-induced DebugMonitor/HardFault sequence. Repeating the observation at 100 kHz produced stable CoreSight and target data. This is treated as a debug-link signal-integrity limitation, not an autonomous firmware fault.

## Root-cause assessment

Without `hc32_ll_icg.c`, the image did not guarantee valid ICG data at `0x00000400`. Code or retained flash contents could therefore be interpreted as device configuration during startup, allowing WDT/SWDT automatic startup. Board-to-board differences can depend on flash history and whether a true power cycle reloads ICG.

The explicit ICG block and link assertions close that build-time gap. The current-board result confirms the fixed image starts and runs without an internal watchdog reset.

## Remaining validation

- Program and verify the same image on the old board.
- Fully power-cycle the old board and confirm ICG readback, reset cause, Thread-mode execution, and zero WDT/SWDT status.

Until that old-board check is completed, cross-board regression coverage is incomplete.
