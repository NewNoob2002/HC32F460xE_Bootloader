# LegacyReference port status

Audit date: 2026-07-13. No production behavior was changed during this audit.

## Evidence inspected

Legacy sources:

- `LegacyReference/liteParse.c` and `.h`
- `LegacyReference/message_decode.c` and `.h`
- `LegacyReference/i2c.c` and `.h`
- `LegacyReference/mcu_config.h`
- `LegacyReference/bsp_clock.c`

Current sources:

- `Core/Src/main.c`, `Core/Inc/boot_config.h`, `Core/Src/boot_state.c`
- `Protocol/Src/legacy_protocol.c`, `legacy_parser.c`, `legacy_codec.c`, and headers
- `Drivers/BSP/Src/bsp_i2c_slave.c` and header
- `App/Src/app_validator.c`, `app_jump.c`, and headers
- `Core/Inc/boot_memory_map.h`, `Core/Src/boot_memory_map.c`
- `Drivers/hc32f4xx_ll_drivers/src/hc32_ll_efm.c` and header
- `HC32F460xE.ld`, `CMakeLists.txt`, `CMakePresets.json`, and host tests

Repository history was searched at every commit for `APP_FLAG`,
`BOOT_PARA_ADDRESS`, `APP_STATUS_SECTOR`, `FLASH_PageNumber`,
`FlashEraseSector`, and `FlashWritePage`. The surrounding workspace was also
searched for the protocol sync, command names, and uploader implementation.

## Comparison

| Legacy behavior | Current implementation | Difference | Required minimal change | Evidence |
|---|---|---|---|---|
| I2C1 slave at `0x11`, PA3/PA2, INT005/6/4 | Implemented and physically reaches Recovery | Current transport fixes legacy ring-buffer and ISR safety defects | Preserve | `LegacyReference/i2c.c`; `Drivers/BSP/Src/bsp_i2c_slave.c` |
| Streaming frame format and legacy CRC | Safe bounded parser and CRC are implemented | Parser internals differ, wire contract is compatible | Preserve safe parser; do not restore unsafe casts/callback singleton | `LegacyReference/liteParse.*`; `Protocol/Src/legacy_parser.c` |
| HANDSHAKE copies request, sets byte 8 to OK, recalculates payload CRC | Implemented and golden-vector tested | None found | Preserve | `message_decode.c:39-48`; `legacy_codec.c` |
| ERASE validates DATA type/start address, calls `FLASH_PageNumber(address)`, erases from sector `BOOT_SIZE/EFM_SECTOR_SIZE` | Not dispatched; gate is off | Page-count/address meaning and old Flash adapter are missing | Recover `FLASH_PageNumber` and uploader ERASE field before porting | `message_decode.c:50-84` |
| APP_DOWNLOAD uses absolute request address, length=`payload_len-12`, data at frame offset 19, writes synchronously | Not dispatched; gate is off | Old `FlashWritePage` is missing; old code lacks end-range and readback checks | Port only after erase/session semantics are known; add mandatory safe end guard/readback | `message_decode.c:87-114` |
| JUMP writes `APP_FLAG` to `BOOT_PARA_ADDRESS`, prepares same-length ACK, sets `BOOT_DELAY_TIME=50` | Not dispatched; gate is off | Marker value/address, delay units, old main-loop finalization, and marker failure behavior are missing | Recover definitions and old main/uploader behavior before porting | `message_decode.c:116-128` |
| APP_UPLOAD command ID exists | No handler in legacy switch; current gate off | No observable implementation | Keep disabled | `liteParse.h`; default case in `message_decode.c` |
| Legacy RX parser is called from polling after STOP | Current parser/service are polled after STOP | Current implementation avoids the legacy half-processing bug | Preserve | `i2c.c:slave_i2c_update`; current `legacy_protocol_service_poll` |
| Recovery jump is caused indirectly by legacy finalization/delay | Current `main` clears `jump_requested` in Recovery | Current loop would block a proven valid Recovery jump | Change only after the legacy JUMP signal/timing is recovered | `main.c`; missing legacy main |

## Current feature gates

The current production configuration is intentionally non-destructive:

```text
BOOT_ENABLE_I2C_SLAVE        1
BOOT_ENABLE_LEGACY_PROTOCOL  1
BOOT_ENABLE_LEGACY_HANDSHAKE 1
BOOT_ENABLE_LEGACY_ERASE     0
BOOT_ENABLE_LEGACY_DOWNLOAD  0
BOOT_ENABLE_LEGACY_JUMP      0
BOOT_ENABLE_LEGACY_UPLOAD    0
BOOT_ENABLE_FLASH_UPDATE     0
```

`legacy_protocol_service_poll()` currently dispatches only command `0x20`.
No reachable `EFM_Program` or `EFM_SectorErase` symbol is present in the linked
Debug ELF because section garbage collection removes the unused DDL routines.

## Exact missing legacy evidence

| Missing item | References | Why required | Behavior not provable |
|---|---|---|---|
| `FLASH_PageNumber()` | ERASE handler | Converts the uploader's address field into erase count | Meaning of ERASE address and exact sector count |
| `FlashEraseSector()` | ERASE handler | Old function takes an apparent sector index, unlike current DDL byte address | Unlock/mode/cache/error/watchdog contract and return behavior |
| `FlashWritePage()` | DOWNLOAD and JUMP | Old low-level program adapter | Program alignment, tail behavior, error handling, and marker write |
| `APP_FLAG` | JUMP handler | Persistent value written before delayed jump | Required compatibility value |
| `BOOT_PARA_ADDRESS` | JUMP handler | Persistent marker destination | Whether it overlaps Boot, App, or reserved Flash |
| `APP_STATUS_SECTOR` | Commented erase call | Indicates an old status-sector design | Marker sector and erase policy |
| `BOOT_DELAY_TIME` consumer | JUMP sets it to 50 | Controls ACK-before-jump timing | Units and precise jump condition |
| `u32FlashAddr_Erase` consumer | ERASE stores request address | May affect completion/finalization | Purpose and later policy |
| Linux uploader | Not present | Defines ERASE field, response timeout, ordering, retry and response lengths | Host-visible destructive command contract |

All Git revisions were checked. `LegacyReference` was first added in commit
`a1c7edc`; the unresolved references were already present in that initial copy.
No repository remote or alternate branch contains an older complete project.

## Stop decision

The explicit stop conditions apply:

- old ERASE semantics cannot be recovered;
- required marker value/address are missing;
- Linux finalization and the 50-unit delay are ambiguous;
- the legacy protocol requires an undocumented persistent flag.

Enabling ERASE, DOWNLOAD, or JUMP would therefore require inventing behavior or
could erase/program an unproven range. These gates remain disabled. No Metadata
or replacement persistence design was introduced.

