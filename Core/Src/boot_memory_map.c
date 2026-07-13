#include "boot_memory_map.h"

bool boot_memory_range_contains(uint32_t begin, uint32_t end, uint32_t address, uint32_t size) {
    return (begin <= end) && (address >= begin) && (address < end) && (size <= (end - address));
}

bool boot_memory_is_sram_address(uint32_t address) {
    return boot_memory_range_contains(HC32_SRAM_BASE, HC32_SRAM_END, address, 1U)
           || boot_memory_range_contains(HC32_RET_SRAM_BASE, HC32_RET_SRAM_END, address, 1U);
}

bool boot_memory_is_valid_initial_msp(uint32_t address) {
    return ((address > HC32_SRAM_BASE) && (address <= HC32_SRAM_END))
           || ((address > HC32_RET_SRAM_BASE) && (address <= HC32_RET_SRAM_END));
}

bool boot_memory_is_app_address(uint32_t address) {
    return boot_memory_range_contains(APP_FLASH_BASE, APP_FLASH_END, address, 1U);
}
