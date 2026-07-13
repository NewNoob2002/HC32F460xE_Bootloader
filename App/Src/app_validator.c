#include "app_validator.h"
#include <stddef.h>
#include "boot_memory_map.h"

static uint32_t read_target_word(uint32_t address, void* context) {
    (void)context;
    return *(const volatile uint32_t*)(uintptr_t)address;
}

bool app_validator_check(uint32_t app_base, app_vector_read_t read_word, void* context) {
    uint32_t msp;
    uint32_t reset;
    uint32_t reset_address;
    if ((read_word == NULL) || !boot_memory_range_contains(APP_FLASH_BASE, APP_FLASH_END, app_base, 8U)
        || ((app_base & 0x3FFUL) != 0U)) {
        return false;
    }
    msp = read_word(app_base, context);
    reset = read_word(app_base + 4U, context);
    if ((msp == UINT32_MAX) || ((msp & 0x7U) != 0U) || !boot_memory_is_valid_initial_msp(msp)) {
        return false;
    }
    if ((reset == UINT32_MAX) || ((reset & 1U) == 0U)) {
        return false;
    }
    reset_address = reset & UINT32_C(0xFFFFFFFE);
    return boot_memory_is_app_address(reset_address);
}

bool boot_application_vector_is_valid(void) {
    return app_validator_check(APP_FLASH_BASE, read_target_word, NULL);
}
