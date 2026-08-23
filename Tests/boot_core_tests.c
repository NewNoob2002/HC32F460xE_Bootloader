#include "app_validator.h"
#include "boot_app.h"
#include "boot_memory_map.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    uint32_t base;
    uint32_t msp;
    uint32_t reset;
} fake_vector_t;

static uint32_t fake_read_word(uint32_t address, void* context) {
    const fake_vector_t* vector = context;
    assert(vector != NULL);
    assert((address == vector->base) || (address == vector->base + 4U));
    return (address == vector->base) ? vector->msp : vector->reset;
}

static void test_memory_ranges(void) {
    assert(boot_memory_range_contains(APP_FLASH_BASE, APP_FLASH_END, APP_FLASH_BASE, 1U));
    assert(boot_memory_range_contains(APP_FLASH_BASE, APP_FLASH_END, APP_FLASH_END - 4U, 4U));
    assert(!boot_memory_range_contains(APP_FLASH_BASE, APP_FLASH_END, APP_FLASH_END, 1U));
    assert(!boot_memory_range_contains(APP_FLASH_BASE, APP_FLASH_END, APP_FLASH_END - 3U, 4U));
    assert(!boot_memory_range_contains(APP_FLASH_BASE, APP_FLASH_END, UINT32_MAX - 1U, 4U));
    assert(boot_memory_is_valid_initial_msp(HC32_SRAM_END));
    assert(!boot_memory_is_valid_initial_msp(HC32_SRAM_BASE));
    assert(boot_memory_is_app_address(APP_FLASH_BASE));
    assert(!boot_memory_is_app_address(APP_FLASH_END));
}

static void test_vector_validation(void) {
    fake_vector_t vector = {
        .base = APP_FLASH_BASE,
        .msp = HC32_SRAM_BASE + 0x100U,
        .reset = APP_FLASH_BASE + 0x101U,
    };

    assert(app_validator_check(vector.base, fake_read_word, &vector));
    assert(!app_validator_check(vector.base, NULL, &vector));

    vector.msp = UINT32_MAX;
    assert(!app_validator_check(vector.base, fake_read_word, &vector));
    vector.msp = HC32_SRAM_BASE + 0x104U;
    assert(!app_validator_check(vector.base, fake_read_word, &vector));
    vector.msp = HC32_SRAM_BASE + 0x100U;

    vector.reset = APP_FLASH_BASE + 0x100U;
    assert(!app_validator_check(vector.base, fake_read_word, &vector));
    vector.reset = APP_FLASH_END | 1U;
    assert(!app_validator_check(vector.base, fake_read_word, &vector));
    vector.reset = APP_FLASH_BASE + 0x101U;

    vector.base = APP_FLASH_BASE + 4U;
    assert(!app_validator_check(vector.base, fake_read_word, &vector));
}

static void test_mode_selection(void) {
    boot_context_t context = {0};

    boot_app_select_mode(&context, false, false);
    assert(context.mode == BOOT_MODE_RECOVERY);
    boot_app_select_mode(&context, true, true);
    assert(context.mode == BOOT_MODE_UPDATE_WINDOW);
    boot_app_select_mode(&context, true, false);
    assert(context.mode == BOOT_MODE_START_APPLICATION);
}

int main(void) {
    test_memory_ranges();
    test_vector_validation();
    test_mode_selection();
    puts("boot_core_tests: PASS");
    return 0;
}
