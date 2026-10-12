#ifndef FIRMWARE_LAYOUT_H
#define FIRMWARE_LAYOUT_H

/* Boot/App 共用的 HC32F460xE 地址契约；Boot 固定占用前 32 KiB。 */
#define BOOT_FLASH_BASE              0x00000000UL
#define BOOT_FLASH_SIZE              0x00008000UL
#define APP_FLASH_BASE               0x00008000UL
#define APP_FLASH_END                0x0007A000UL
#define APP_FLASH_MAX_SIZE           (APP_FLASH_END - APP_FLASH_BASE)
#define BOOT_METADATA_A_BASE         0x0007A000UL
#define BOOT_METADATA_B_BASE         0x0007C000UL
#define BOOT_METADATA_SECTOR_SIZE    0x00002000UL
#define BOOT_RESERVED_SECTOR_BASE    0x0007E000UL
#define PHYSICAL_FLASH_END_EXCLUSIVE 0x00080000UL
#if (APP_FLASH_BASE != (BOOT_FLASH_BASE + BOOT_FLASH_SIZE)) || (BOOT_FLASH_SIZE != 0x00008000UL)                       \
    || ((APP_FLASH_BASE % BOOT_METADATA_SECTOR_SIZE) != 0U) || ((APP_FLASH_END % BOOT_METADATA_SECTOR_SIZE) != 0U)     \
    || (BOOT_METADATA_A_BASE != APP_FLASH_END)                                                                         \
    || (BOOT_METADATA_B_BASE != (BOOT_METADATA_A_BASE + BOOT_METADATA_SECTOR_SIZE))                                    \
    || (BOOT_RESERVED_SECTOR_BASE != (BOOT_METADATA_B_BASE + BOOT_METADATA_SECTOR_SIZE))                               \
    || (PHYSICAL_FLASH_END_EXCLUSIVE != (BOOT_RESERVED_SECTOR_BASE + BOOT_METADATA_SECTOR_SIZE))
#error "Boot/App Flash boundaries must be contiguous, 8 KiB aligned, with a 32 KiB Boot"
#endif
#define HC32_SRAM_BASE     0x1FFF8000UL
#define HC32_SRAM_END      0x20027000UL
#define HC32_RET_SRAM_BASE 0x200F0000UL
#define HC32_RET_SRAM_END  0x200F1000UL

#endif
