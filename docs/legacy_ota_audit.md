# Legacy OTA audit

Historical audit snapshot. Current reachable behavior is documented in [current_status.md](current_status.md); real erase/program operations were added after this audit.

Inspected `LegacyReference/liteParse.c/.h`, `message_decode.c/.h`, `i2c.c/.h`,
and `mcu_config.h`. They establish frame constants, commands, CRC, I2C1 pins,
and address, but do not establish a safe persistent application-marker layout.

| Item | Current value | Source | Confidence | Notes |
|---|---|---|---|---|
| MCU | HC32F460xE | linker/device headers | confirmed | |
| Flash | 512 KiB | `HC32F460xE.ld` | confirmed | `0x00000000..0x0007FFFF` |
| Boot region | 32 KiB | linker assertions | confirmed | |
| Application base | `0x00008000` | `Core/Inc/boot_memory_map.h` | confirmed | unchanged |
| Sector size | `0x2000` (8 KiB) | `hc32_ll_efm.h` | confirmed | |
| Program unit | 4 bytes in current BSP | `Drivers/BSP/Src/bsp_flash.c` | confirmed for current port | legacy wrapper remained unknown |
| APP_FLAG | undefined | legacy references only | unknown | OTA blocked |
| BOOT_PARA_ADDRESS | undefined | legacy references only | unknown | OTA blocked |
| APP_STATUS_SECTOR | undefined | commented legacy code | unknown | OTA blocked |
| FLASH_PageNumber | undefined | legacy reference only | unknown | erase semantics unknown |
| I2C | CM_I2C1 | `LegacyReference/i2c.h` | confirmed | |
| I2C address | `0x11` | legacy headers | confirmed | |

The old `message_decode.c` remains excluded. A new `boot_update_service.c` now performs bounded Application-region erase/download with per-chunk readback, but does not implement persistent markers or whole-image integrity.
