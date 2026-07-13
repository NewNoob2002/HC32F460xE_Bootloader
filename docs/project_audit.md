# Current project audit

Audit date: 2026-07-13. Repository revision: `814873c` (`main`).

The checked-out repository is not the legacy bootloader described in the task. It contains one commit, the HC32 CMSIS/DDL packages, a partial application-oriented CMake file, and four `Core` files. The referenced `Platform`, `Arduino`, `Simulator/App`, and `USER` source trees are absent. No `liteParse`, `message_decode`, bootloader `i2c`, board schematic, IDE project, debug configuration, or flash command is present. Values that cannot be established are therefore recorded as `UNKNOWN`; no protocol ID, ACK, pin, IRQ route, or board clock is inferred.

| Item | Current value | Source file | Confidence | Notes |
| ---- | ------------- | ----------- | ---------- | ----- |
| MCU part number | HC32F460xE family | `HC32F460xE.ld` | High | Exact package/suffix beyond xE is UNKNOWN. |
| Flash size | 512 KiB | `HC32F460xE.ld` | High | Linker header and region length agree. |
| RAM size | 192 KiB total | `HC32F460xE.ld` | High | 188 KiB main SRAM plus 4 KiB retention RAM. |
| Boot Flash base | `0x00000000` | vendor linker default | High | Intended boot region inferred from the preserved application base. |
| Boot size | 32 KiB (`0x8000`) | top-level CMake application origin | High | Boundary is the confirmed application base. |
| Application base | `0x00008000` | `CMakeLists.txt`, `system_hc32f460.c` | High | Both linker override and VTOR use it. |
| Application maximum size | 480 KiB (`0x78000`) | derived from confirmed regions | High | Ends at `0x00080000`; no smaller current limit exists. |
| Metadata/application flag address | `UNKNOWN` | absent legacy sources | None | Must be supplied from the legacy repository/schematic. |
| Flash sector size | 8 KiB (`0x2000`) | `hc32_ll_efm.h` | High | `EFM_SECTOR_SIZE`. |
| Flash programming unit | `UNKNOWN` | DDL API only | Low | DDL exposes word-oriented calls, but hardware contract was not independently established. |
| I2C peripheral | I2C1 (requirement only) | task specification | Medium | No board implementation exists in this checkout. |
| I2C pins | `UNKNOWN` | absent board sources/schematic | None | Must not be invented. |
| I2C slave address | 7-bit `0x11` | task specification | High | Preserved centrally. |
| I2C IRQ sources | RXI 420, TXI 421, TEI 422, EEI 423 | `hc32f460.h` | High | NVIC channel routing is programmable and currently UNKNOWN. |
| Main clock source | Reset-state HRC; board configuration `UNKNOWN` | `system_hc32f460.c` | Medium | Existing `SystemInit` only reads the active clock. |
| Main clock frequency | 16 or 20 MHz HRC at reset | `system_hc32f460.c` | Medium | Selected from the HRC monitor bit; board target is UNKNOWN. |
| Compiler | GNU Arm Embedded GCC | CMake toolchain file | High | Installed version recorded by the build report. |
| Linker script | root `HC32F460xE.ld` | `CMakeLists.txt` | High | It is a modified vendor script. |
| Binary output format | ELF, Intel HEX, raw BIN | `CMakeLists.txt` | High | Post-build commands exist but current configure fails. |
| Current Boot image size | `UNKNOWN` | no boot artifact | None | Existing ignored build was configured with host GCC and has no firmware. |

## Structure and build findings

- Startup: `Drivers/CMSIS/Device/HDSC/hc32f4xx/Source/GCC/startup_hc32f460.S`.
- System initialization: `Drivers/CMSIS/Device/HDSC/hc32f4xx/Source/system_hc32f460.c`.
- Vector table: 16 Cortex exceptions plus 144 programmable HC32 IRQ entries in the startup assembly.
- Current interrupt source: `Core/src/hc32f4xx_it.c`; it depends on absent logging, HAL, LVGL, and application symbols and is not usable in a minimal boot.
- Device/CMSIS: `Drivers/CMSIS/Device/HDSC/hc32f4xx` and `Drivers/CMSIS/Include`.
- Peripheral library: `Drivers/hc32f4xx_ll_drivers`.
- Toolchain flags: Cortex-M4, Thumb, FPv4-SP-D16 hard-float; section GC; nano/nosys specs; C11/C++17.
- Current warnings: `-Wall -Wextra`; requested stricter project warnings are not present.
- Heap and stack: 8 KiB each in startup assembly. Dynamic allocation is not needed by the baseline.
- Post-build: objcopy to HEX/BIN and `size` output.
- Flash/debug method: `UNKNOWN`; only vendor FLM files and SVDs are present.
- IDE projects: none present.

## Audit risks

1. The checked-in CMake file references missing directories and cannot represent a clean-checkout build.
2. Its `FLASH_ORIGIN=0x00008000` makes it an application link, not a bootloader link.
3. The root linker script leaves `FLASH_LENGTH` at 512 KiB when origin is moved, which would extend past physical Flash unless overridden.
4. `SystemInit` hard-codes VTOR to `0x8000`, inappropriate for a boot image at zero.
5. No legacy protocol implementation exists, so exact command IDs, offsets, ACKs, response bytes, and transaction behavior cannot be verified.
6. No board pin map exists, so physically enabling I2C1 would require invented GPIO data and is prohibited.

