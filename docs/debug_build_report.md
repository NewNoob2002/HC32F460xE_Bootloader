# Debug build and hardware regression report

Historical evidence for the artifact hashes and physical session recorded here. It is not the current build report; see [build_report.md](build_report.md) and [current_status.md](current_status.md).

## Artifact

- Preset: `Debug`
- Compiler: GNU Arm Embedded 14.3.1
- Flags: `-Og -g3 -fno-omit-frame-pointer -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard`
- ELF: `build/Debug/hc32f460_boot.elf`
- ELF SHA-256: `786ac758cead41255fa15f66af9b7d74ba56cdd60f6d54ca402c369ebd8253e3`
- BIN SHA-256: `c235409c2ec2ddd102138848bc4df55f5014ed40b001f9d47b28408ecb7a6f79`
- Flash: 19,604 bytes of 32 KiB (59.83%), 13,164 bytes free
- Initialized RAM: 80 bytes plus 168-byte RTT control block
- BSS: 5,032 bytes
- Linker RAM use: 5,280 bytes
- Heap/stack reservation: 2,048 bytes

The clean rebuild reproduced the expected ELF SHA exactly. Host tests passed
1/1. ELF, BIN, HEX, and MAP outputs were generated.

## Probe and programming

J-Link Commander/DLL V9.50 connected over SWD at 1 MHz. The probe reported
firmware from 2021-05-07, hardware V9.20, VTref 3.275-3.280 V, and serial `-1`.
The installed exact package device entry is `HC32F460PETB-LQFP100`; Commander
normalizes its display to `HC32F460XE`.

The V9.50 built-in HC32 Flash loader reported download/verification success but
a fresh connection still read the old image. This was proven by an initial
`compare-sections` mismatch. A repository-derived SRAM EFM helper then erased
and programmed Boot sectors 0-2 only, verified every word, and reported status
`magic=0x45464D32`, `stage=0x600D600D`, `error=0`. Application Flash, metadata,
and sector 63 were not touched. A fresh connection and GDB `compare-sections`
then matched every allocatable ELF section.

## Runtime regression

- CPACR after enable/barriers: `0x00F00000`
- VTOR after write/barriers: `0x00000000`
- PRIMASK at main: `0`
- First I2C VFP instruction: `0x00002354`, `vmov s15, ip`
- CPACR before/after that instruction: `0x00F00000`
- Recovery `bsp_i2c_slave_poll()` reached
- I2C NVIC enable bits: `0x00000070` (IRQ4/5/6)
- NVIC pending bits: zero
- CFSR/HFSR: zero
- `g_boot_fault_snapshot.magic`: zero
- Reset regression: 20/20 resets reached live Thread-mode code with no fault

The final CPU was resumed. A later attempt to open another GDB server failed
because the J-Link USB interface was no longer accessible (`Could not connect
to J-Link`; `lsusb` returned libusb error -99). This does not invalidate the
saved successful session, but the physical USB/probe state should be checked
before the next session.

RTT control-block memory was present at `0x1FFF8050`, but JLinkRTTLogger did not
attach to it in this session. Runtime electrical PB3/PA6/PB5 waveforms and the
Linux I2C handshake were not measured here.
