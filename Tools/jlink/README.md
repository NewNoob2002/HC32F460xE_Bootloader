# J-Link workflows

These scripts target the HC32F460xE pin-revised test board through J-Link
serial `63728769`, SWD at 100 kHz. Run them from the repository root.

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
| `test_board_capture_i2c1_postread.jlink` | Capture I2C1 post-read counters/state | running |
| `read_flash_security.jlink` | Read ICG/security/EFM state | running |
| `hardfault.gdb`, `live_attach.gdb` | Symbol-aware GDB diagnosis | script-specific |

Example:

```sh
JLinkExe -NoGui 1 -ExitOnError 1 \
  -CommandFile Tools/jlink/test_board_verify_debug.jlink
```

`test_board_capture_i2c1_postread.jlink` contains RAM addresses from the current
Debug ELF. Before reuse after a rebuild, confirm them with:

```sh
arm-none-eabi-nm -n -S build/Debug/hc32f460_boot.elf | \
  rg 'i2c_slave_counters_stats|tx_transaction_started_bytes|tx_transactions|m_apfnIrqHandler'
```

Raw safety preflights and lab transcripts are local scratch data under the
ignored `debug_artifacts/` directory. Durable results belong in project docs.

## X1 HC32F460JEUA

The current X1 board uses HC32F460JEUA. J-Link V9.76a lists the corresponding
512 KiB family entry `HC32F460xE`; the older scripts above retain their historical
package names and target evidence.

| Script | Purpose | Final state |
| --- | --- | --- |
| `x1_flash_debug.jlink` | Program and verify Boot at 0 and X1 App at `0x8000` | halted |
| `x1_verify_run_debug.jlink` | Fresh-session reset/halt, verify both BINs, then reset/run | running |

Build Debug and check both BIN hashes/addresses first. Stop any active J-Link
GDB Server before opening Commander. Preserve the current Flash and calibration
before overwriting; neither script mass-erases or touches the last three sectors.
The second script deliberately verifies after reset/halt: a previous verification
in an inherited running context gave a transient mismatch.

```sh
cmake --preset Debug
cmake --build --preset Debug
sha256sum build/Debug/hc32f460_boot.bin build/Debug/hc32f460_app_x1.bin
JLinkExe -NoGui 1 -ExitOnError 1 -CommandFile Tools/jlink/x1_flash_debug.jlink
JLinkExe -NoGui 1 -ExitOnError 1 -CommandFile Tools/jlink/x1_verify_run_debug.jlink
```

Runtime diagnosis loads `hc32f460_app_x1.elf` as symbols only. Use its actual
`_SEGGER_RTT` symbol rather than an address from a prior build. HIL results and
limits are recorded in [X1 test version](../../docs/x1_hil_test_version.md).
