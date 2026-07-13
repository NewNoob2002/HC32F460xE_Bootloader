# Legacy Flash contract

Flash update is not enabled in this revision. The reference calls
`FlashEraseSector`, `FlashWritePage`, and `FLASH_PageNumber`, but their project
definitions and the persistent marker addresses (`APP_FLAG`,
`BOOT_PARA_ADDRESS`, `APP_STATUS_SECTOR`) are absent from this checkout.

The bootloader therefore cannot prove marker placement, decode the legacy erase
address, or guarantee that an erase will not destroy unrelated data. No erase,
program, or marker write is reachable in the production target. The application
base remains `0x00008000`.

Before enabling OTA, provide the exact marker address/sector and values,
program-unit and cache/RAM execution requirements, and erase-address semantics.

