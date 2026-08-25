# J-Link workflows

These scripts target the HC32F460xE pin-revised test board through J-Link
serial `20781318`, SWD at 100 kHz. Run them from the repository root.

Create a hardware safety preflight before Flash, reset/run or halt operations.
For a raw BIN, verify its SHA-256, size and load address `0x00000000` first.
The scripts never unlock protection, recover the device or mass erase.

| Script | Purpose | Final target state |
|---|---|---|
| `show_probe_config.jlink` | Confirm probe and target identity | unchanged |
| `test_board_flash_debug.jlink` | Program and verify the Debug BIN | halted |
| `test_board_verify_debug.jlink` | Fresh-session BIN verification | unchanged |
| `test_board_reset_run.jlink` | Reset and run a verified image | running |
| `connect_halt.jlink` | Capture core/fault registers | halted |
| `test_board_capture_i2c1_postread.jlink` | Capture node 1 I2C1 counters | running |
| `read_flash_security.jlink` | Read ICG/security/EFM state | running |
| `hardfault.gdb`, `live_attach.gdb` | Symbol-aware GDB diagnosis | script-specific |

Example:

```sh
JLinkExe -NoGui 1 -ExitOnError 1 \
  -CommandFile Tools/jlink/test_board_verify_debug.jlink
```

`test_board_capture_i2c1_postread.jlink` contains RAM addresses from the node 1
Debug ELF. Before reuse after a rebuild, confirm them with:

```sh
arm-none-eabi-nm -n -S build/Debug/hc32f460_boot.elf | \
  rg 'i2c_slave_counters_stats|tx_transaction_started_bytes|tx_transactions|m_apfnIrqHandler'
```

Raw safety preflights and lab transcripts are local scratch data under the
ignored `debug_artifacts/` directory. Durable results belong in project docs.
