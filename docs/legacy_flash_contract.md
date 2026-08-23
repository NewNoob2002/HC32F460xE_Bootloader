# Legacy Flash contract

Historical recovery note. Current reachable behavior is documented in [current_status.md](current_status.md) and [hc32_efm_contract.md](hc32_efm_contract.md).

At the time of this legacy audit, Flash update was not enabled. The reference calls
`FlashEraseSector`, `FlashWritePage`, and `FLASH_PageNumber`, but their project
definitions and the persistent marker addresses (`APP_FLAG`,
`BOOT_PARA_ADDRESS`, `APP_STATUS_SECTOR`) are absent from this checkout.

The current implementation no longer waits for those missing wrappers: it erases the complete configured Application region from `0x00008000` to `0x00079FFF`, programs validated in-range addresses and performs per-chunk readback. It still has no compatible persistent marker or complete-image validity state.

Before enabling OTA, provide the exact marker address/sector and values,
program-unit and cache/RAM execution requirements, and erase-address semantics.
