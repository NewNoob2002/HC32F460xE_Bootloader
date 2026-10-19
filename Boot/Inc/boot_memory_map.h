#ifndef BOOT_MEMORY_MAP_H
#define BOOT_MEMORY_MAP_H

#include <stdbool.h>
#include <stdint.h>

#include "firmware_layout.h"

bool boot_memory_range_contains(uint32_t begin, uint32_t end, uint32_t address, uint32_t size);
bool boot_memory_is_sram_address(uint32_t address);
bool boot_memory_is_valid_initial_msp(uint32_t address);
bool boot_memory_is_app_address(uint32_t address);

#endif
