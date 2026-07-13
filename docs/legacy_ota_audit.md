# Legacy OTA audit

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
| Program unit | not proven | — | unknown | OTA blocked |
| APP_FLAG | undefined | legacy references only | unknown | OTA blocked |
| BOOT_PARA_ADDRESS | undefined | legacy references only | unknown | OTA blocked |
| APP_STATUS_SECTOR | undefined | commented legacy code | unknown | OTA blocked |
| FLASH_PageNumber | undefined | legacy reference only | unknown | erase semantics unknown |
| I2C | CM_I2C1 | `LegacyReference/i2c.h` | confirmed | |
| I2C address | `0x11` | legacy headers | confirmed | |

The old `message_decode.c` performs unchecked Flash operations and is not
compiled into the production target.

