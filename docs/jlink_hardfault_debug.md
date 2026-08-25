# J-Link HardFault workflow

Rebuild and verify the current ELF before each session; see [current_status.md](current_status.md) and [`Tools/jlink/README.md`](../Tools/jlink/README.md).

## Known-good setup

- J-Link Commander/DLL: V9.70
- Device entry: `HC32F460PETB-LQFP100` (displayed as `HC32F460XE`)
- Interface: SWD
- Speed: 100 kHz
- Debug ELF: `build/Debug/hc32f460_boot.elf`
- Node 1 validated SHA-256: `cc41f67e0f52884ed9d39b262b8537c39c06e54fdb809f4c8d1f5d097e9f8422`

Build with:

```sh
cmake --preset Debug --fresh
cmake --build --preset Debug
sha256sum build/Debug/hc32f460_boot.elf
```

Use the same ELF for both programming and GDB symbols. Run `compare-sections`
before interpreting any source location.

## Programming verification

The node 1 flow uses the standard SEGGER HC32 Flash loader, verifies the BIN in
the programming session and repeats `verifybin` in a fresh session before
reset/run. The old hard-coded RAM-loader workaround was removed. Do not add an
unlock, recovery or mass-erase step to HardFault diagnosis.

## GDB capture

Start the server:

```sh
JLinkGDBServerCLExe -device HC32F460PETB-LQFP100 -if SWD -speed 100 \
  -port 2331 -swoport 2332 -telnetport 2333 -singlerun
```

Then use the checked-in batch workflow or an interactive GDB session:

```sh
arm-none-eabi-gdb -q build/Debug/hc32f460_boot.elf
(gdb) target remote :2331
(gdb) monitor reset
(gdb) monitor halt
(gdb) compare-sections
```

Break at `SystemInit`, `main`, `bsp_i2c_slave_init`, `HardFault_Handler` and
`boot_hardfault_capture`. Keep halt intervals short so the external TPL5010
does not obscure the result.

At a new fault, read `g_boot_fault_snapshot` and use its `stacked_pc`, not the
current handler PC. Decode all CFSR fields. Only use MMFAR or BFAR when their
validity bits are set. Symbolize the stacked PC, PC-2, PC-4, and stacked LR
against the exact compared ELF.

If no fault occurs, record that result instead of inventing a stacked frame.
