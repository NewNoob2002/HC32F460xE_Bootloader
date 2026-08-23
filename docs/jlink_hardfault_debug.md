# Reproducible J-Link HardFault workflow

The versions, hashes and probe observations below reproduce a historical session. Rebuild and verify the current ELF before reuse; see [current_status.md](current_status.md).

## Known-good setup

- J-Link Commander/DLL: V9.50
- Device entry: `HC32F460PETB-LQFP100` (displayed as `HC32F460XE`)
- Interface: SWD
- Speed: 1000 kHz
- Debug ELF: `build/Debug/hc32f460_boot.elf`
- Expected SHA-256: `786ac758cead41255fa15f66af9b7d74ba56cdd60f6d54ca402c369ebd8253e3`

Build with:

```sh
cmake --preset Debug --fresh
cmake --build --preset Debug
sha256sum build/Debug/hc32f460_boot.elf
```

Use the same ELF for both programming and GDB symbols. Run `compare-sections`
before interpreting any source location.

## Important local J-Link programming caveat

On the tested V9.50 installation/probe, the built-in HC32 Flash loader reported
successful programming but a fresh connection still saw the prior firmware.
The failed and successful evidence is preserved in `debug_artifacts/jlink/`.
Do not trust an immediate-session verify alone on this setup.

`Tools/jlink/run_ram_flash_loader.jlink` and
`Tools/jlink/ram_flash_loader.c` are the narrow recovery path used in this
session. They program Boot sectors only. Review the ELF SHA and loader address
limits before reusing them; never broaden them to application or metadata
regions as part of HardFault diagnosis.

## GDB capture

Start the server:

```sh
JLinkGDBServerCLExe -device HC32F460PETB-LQFP100 -if SWD -speed 1000 \
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

Break at `SystemInit`, `main`, `GPIO_ReadOutputPins`,
`bsp_i2c_slave_init`, the first VFP instruction in `I2C_BaudrateConfig`,
`HardFault_Handler`, and `boot_hardfault_capture`. Keep halt intervals short so
the external TPL5010 does not obscure the result.

At a new fault, read `g_boot_fault_snapshot` and use its `stacked_pc`, not the
current handler PC. Decode all CFSR fields. Only use MMFAR or BFAR when their
validity bits are set. Symbolize the stacked PC, PC-2, PC-4, and stacked LR
against the exact compared ELF.

The current corrected build did not fault, so
`debug_artifacts/jlink/fault_snapshot.txt` explicitly records the absence of a
new exception frame rather than inventing stacked values.
