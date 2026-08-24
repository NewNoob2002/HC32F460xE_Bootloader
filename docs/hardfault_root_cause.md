# HardFault root-cause report

Historical investigation evidence. Current capability and verification status are maintained in [current_status.md](current_status.md).

## Outcome

The corrected Debug image does not reproduce the Boot HardFault. It reaches
I2C initialization, executes the first VFP instruction, and enters the Recovery
poll loop. Twenty automated resets produced CFSR `0`, HFSR `0`, and fault
snapshot magic `0` on every iteration.

Because the historical failing image did not preserve a valid exception frame,
the exact historical stacked PC/LR and caller cannot be recovered. Under the
required decision rules, the former NOCP report therefore cannot honestly be
called a proven root cause. The missing CPACR synchronization was a real startup
defect and is the most plausible explanation, but remains a retrospective
hypothesis rather than a captured causal chain.

## Actual runtime evidence

| Check | Captured value |
|---|---:|
| CPACR at `SystemInit` entry | `0x00000000` |
| CPACR after write + DSB/ISB | `0x00F00000` |
| VTOR after write + DSB/ISB | `0x00000000` |
| PRIMASK at `main` | `0` |
| CPACR at `bsp_i2c_slave_init` | `0x00F00000` |
| First I2C VFP instruction | `0x00002354: vmov s15, ip` |
| CPACR before/after first VFP | `0x00F00000` / `0x00F00000` |
| Recovery poll reached | yes, `bsp_i2c_slave_poll` at `0x000014B8` |
| CFSR / HFSR | `0x00000000` / `0x00000000` |
| Fault snapshot magic | `0x00000000` |

The VFP instruction stepped successfully. No current UFSR bit is set, including
NOCP. MMFAR and BFAR are not valid because MMARVALID/BFARVALID are clear.

Runtime `GPIO_ReadOutputPins` arguments were also correct:

| Caller | R0 port | R1 pin | LR |
|---|---:|---:|---:|
| PB3 power hold | `1` | `0x0008` | `0x00000BE3` |
| PA6 watchdog | `0` | `0x0040` | `0x00000D63` |
| PB5 LED | `1` | `0x0020` | `0x00000EF1` |

Thus the current evidence does not support an invalid GPIO argument. The
historical Ozone address near `0x000017DC` was a non-coprocessor `movne` in a
different ELF and was not a trustworthy stacked fault PC.

## Proven image mismatch and programming issue

The first GDB `compare-sections` showed all Boot sections mismatched even though
the J-Link V9.50 HC32 loader had reported success. Immediate-session reads saw
the new bytes, while a fresh connection saw the old firmware. This was a real
ELF/Flash mismatch and made the earlier Ozone address attribution unreliable.

The Boot image was programmed persistently using a small SRAM-resident EFM
helper derived from the repository HC32 DDL. It touched Boot sectors 0-2 only.
Afterward, fresh memory reads matched the vector table and code, and GDB reported
every allocatable ELF section as `matched`.

## Minimum code correction

The startup correction is deliberately narrow:

1. Grant CP10/CP11 full access.
2. Execute DSB and ISB immediately afterward.
3. Write VTOR and execute DSB/ISB.
4. Remove the early `__enable_irq()` from `SystemInit`.
5. Run `SystemInit` before C constructors.
6. Retain the naked HardFault wrapper and persistent fault snapshot for any
   future recurrence.

No GPIO driver workaround, I2C ABI change, Flash-update code, or destructive OTA
operation was introduced as a fault fix.

## Conclusion

The corrected startup sequence fixes an architecturally unsafe CPACR/interrupt
ordering defect, and the corrected image is stable on the physical target. The
current run excludes invalid GPIO arguments, current NOCP/FPU access, a stale
I2C pending interrupt, invalid vector placement, and post-program image mismatch
as active causes. A precise historical root cause cannot be promoted beyond the
CPACR synchronization hypothesis without the old failing image and its original
stacked exception frame.

The raw logs and target dumps were one-session diagnostic artifacts and are not
versioned. Reproduce the checks with the maintained J-Link workflow before
using addresses from a new build.
