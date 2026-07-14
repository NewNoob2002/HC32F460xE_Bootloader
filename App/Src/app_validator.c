#include "app_validator.h"
#include <stddef.h>
#include "boot_log.h"
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
    const uint32_t msp = read_target_word(APP_FLASH_BASE, NULL);
    const uint32_t reset = read_target_word(APP_FLASH_BASE + 4U, NULL);
    const uint32_t reset_address = reset & UINT32_C(0xFFFFFFFE);
    const bool msp_not_erased = msp != UINT32_MAX;
    const bool msp_aligned = (msp & 0x7U) == 0U;
    const bool msp_in_sram = boot_memory_is_valid_initial_msp(msp);
    const bool reset_not_erased = reset != UINT32_MAX;
    const bool reset_is_thumb = (reset & 1U) != 0U;
    const bool reset_in_app = boot_memory_is_app_address(reset_address);
    const bool valid = app_validator_check(APP_FLASH_BASE, read_target_word, NULL);

    BOOT_LOG_INFO("APP_VECTOR base=0x%08lX msp=0x%08lX reset=0x%08lX target=0x%08lX",
                  (unsigned long)APP_FLASH_BASE, (unsigned long)msp, (unsigned long)reset,
                  (unsigned long)reset_address);
    if (!valid) {
        BOOT_LOG_WARN("APP_VECTOR invalid msp:erased=%u align8=%u sram=%u reset:erased=%u thumb=%u app=%u",
                      msp_not_erased ? 0U : 1U, msp_aligned ? 1U : 0U, msp_in_sram ? 1U : 0U,
                      reset_not_erased ? 0U : 1U, reset_is_thumb ? 1U : 0U, reset_in_app ? 1U : 0U);
    }
    return valid;
}
