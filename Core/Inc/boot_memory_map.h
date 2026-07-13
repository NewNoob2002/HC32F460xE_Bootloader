#ifndef BOOT_MEMORY_MAP_H
#define BOOT_MEMORY_MAP_H

#include <stdbool.h>
#include <stdint.h>

#define BOOT_FLASH_BASE    0x00000000UL
#define BOOT_FLASH_SIZE    0x00008000UL
#define APP_FLASH_BASE     0x00008000UL
#define APP_FLASH_END      0x0007A000UL
#define APP_FLASH_MAX_SIZE (APP_FLASH_END - APP_FLASH_BASE)
#define BOOT_METADATA_A_BASE 0x0007A000UL
#define BOOT_METADATA_B_BASE 0x0007C000UL
#define BOOT_METADATA_SECTOR_SIZE 0x00002000UL
#define BOOT_RESERVED_SECTOR_BASE 0x0007E000UL
#define PHYSICAL_FLASH_END_EXCLUSIVE 0x00080000UL
#if ((APP_FLASH_BASE % BOOT_METADATA_SECTOR_SIZE) != 0U) || \
    ((APP_FLASH_END % BOOT_METADATA_SECTOR_SIZE) != 0U) || \
    (BOOT_METADATA_A_BASE != APP_FLASH_END) || \
    (BOOT_METADATA_B_BASE != (BOOT_METADATA_A_BASE + BOOT_METADATA_SECTOR_SIZE)) || \
    (BOOT_RESERVED_SECTOR_BASE != (BOOT_METADATA_B_BASE + BOOT_METADATA_SECTOR_SIZE))
#error "Boot V2 Flash boundaries must be 8 KiB aligned and contiguous"
#endif
#define HC32_SRAM_BASE     0x1FFF8000UL
#define HC32_SRAM_END      0x20027000UL
#define HC32_RET_SRAM_BASE 0x200F0000UL
#define HC32_RET_SRAM_END  0x200F1000UL

bool boot_memory_range_contains(uint32_t begin, uint32_t end, uint32_t address, uint32_t size);
bool boot_memory_is_sram_address(uint32_t address);
bool boot_memory_is_valid_initial_msp(uint32_t address);
bool boot_memory_is_app_address(uint32_t address);

#endif
