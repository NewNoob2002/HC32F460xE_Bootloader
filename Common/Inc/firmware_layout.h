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
/* App 参数区（X1 SOC 补偿）占用原保留扇区；Boot 擦除/下载只覆盖 App 区，参数在升级后保留。
 * Flash BSP 可访问整个物理 Flash，参数存储调用方须自行限制在本区间内。 */
#define APP_PARAM_SECTOR_BASE BOOT_RESERVED_SECTOR_BASE
#define APP_PARAM_SECTOR_SIZE BOOT_METADATA_SECTOR_SIZE
#define APP_PARAM_SECTOR_END  (APP_PARAM_SECTOR_BASE + APP_PARAM_SECTOR_SIZE)
/* HC32 LL hc32_ll_efm.h：EFM_SWAP_ADDR=0x7FFDC 为换 Bank 控制字；从该地址起不得写业务记录。
 * 16 字节记录在此之前仅容纳 509 条，最后一条结束于 0x7FFD0，扇区擦除单位仍为 8 KiB。 */
#define APP_PARAM_DATA_END    0x0007FFDCUL
#if (APP_PARAM_DATA_END <= APP_PARAM_SECTOR_BASE) || (APP_PARAM_DATA_END > APP_PARAM_SECTOR_END)
#error "App parameter data must exclude the Flash swap/control tail"
#endif
#if (APP_PARAM_SECTOR_BASE < (BOOT_METADATA_B_BASE + BOOT_METADATA_SECTOR_SIZE))                                       \
    || (APP_PARAM_SECTOR_END > PHYSICAL_FLASH_END_EXCLUSIVE)
#error "App parameter sector must not overlap the App image or Boot metadata"
#endif
#define HC32_SRAM_BASE     0x1FFF8000UL
#define HC32_SRAM_END      0x20027000UL
#define HC32_RET_SRAM_BASE 0x200F0000UL
#define HC32_RET_SRAM_END  0x200F1000UL

#endif
