#ifndef BOOT_MEMORY_MAP_H
#define BOOT_MEMORY_MAP_H

#include <stdbool.h>
#include <stdint.h>

#define BOOT_FLASH_BASE    0x00000000UL
#define BOOT_FLASH_SIZE    0x00008000UL
#define APP_FLASH_BASE     0x00008000UL
#define APP_FLASH_END      0x00080000UL
#define HC32_SRAM_BASE     0x1FFF8000UL
#define HC32_SRAM_END      0x20027000UL
#define HC32_RET_SRAM_BASE 0x200F0000UL
#define HC32_RET_SRAM_END  0x200F1000UL

bool boot_memory_range_contains(uint32_t begin, uint32_t end, uint32_t address, uint32_t size);
bool boot_memory_is_sram_address(uint32_t address);
bool boot_memory_is_valid_initial_msp(uint32_t address);
bool boot_memory_is_app_address(uint32_t address);

#endif
