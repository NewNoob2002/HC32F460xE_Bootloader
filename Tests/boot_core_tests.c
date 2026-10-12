#include "app_validator.h"
#include "boot_app.h"
#include "boot_config.h"
#include "boot_memory_map.h"
#include "boot_status_led.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static boot_led_mode_t mock_led_mode;
static unsigned mock_led_writes, mock_power_releases, mock_upgrade_resets;

void boot_reset_after_upgrade(void) {
    assert(mock_led_mode == BOOT_LED_MODE_OFF);
    ++mock_upgrade_resets;
}

void boot_status_led_set_mode(boot_led_mode_t mode) {
    mock_led_mode = mode;
    ++mock_led_writes;
}
void boot_status_led_off(void) {
    boot_status_led_set_mode(BOOT_LED_MODE_OFF);
}
void bsp_power_hold_deassert(void) {
    assert(mock_led_mode == BOOT_LED_MODE_OFF);
    ++mock_power_releases;
}

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

    boot_app_select_mode(&context, false, false, false);
    assert(context.mode == BOOT_MODE_RECOVERY);
    boot_app_select_mode(&context, true, true, false);
    assert(context.mode == BOOT_MODE_UPDATE_WINDOW);
    boot_app_select_mode(&context, true, false, false);
    assert(context.mode == BOOT_MODE_START_APPLICATION);
    boot_app_select_mode(&context, true, true, true);
    assert(context.mode == BOOT_MODE_START_APPLICATION);
    boot_app_select_mode(&context, false, true, true);
    assert(context.mode == BOOT_MODE_RECOVERY);
}

static void test_led_mode_timing(void) {
    led_scheduler_t led;
    led_scheduler_init(&led, true, NULL, NULL);
    assert(led.mode == BOOT_LED_MODE_OFF && !led.logical_on && led.interval_ms == 0U);
    led_scheduler_set_mode(&led, BOOT_LED_MODE_UPDATE_WINDOW, 100U);
    led_scheduler_set_mode(&led, BOOT_LED_MODE_UPDATE_WINDOW, 200U);
    led_scheduler_poll(&led, 349U);
    assert(!led.logical_on && led.previous_ms == 100U);
    led_scheduler_poll(&led, 350U);
    assert(led.logical_on);
    led_scheduler_set_mode(&led, BOOT_LED_MODE_UPDATE_WINDOW, 400U);
    assert(led.logical_on && led.previous_ms == 350U);
    led_scheduler_poll(&led, 600U);
    assert(!led.logical_on);
    led_scheduler_set_mode(&led, BOOT_LED_MODE_RECOVERY, 700U);
    assert(!led.logical_on && led.previous_ms == 700U && led.interval_ms == 1000U);
    led_scheduler_poll(&led, 1699U);
    assert(!led.logical_on);
    led_scheduler_poll(&led, 1700U);
    assert(led.logical_on);
    led_scheduler_init(&led, true, NULL, NULL);
    assert(led.mode == BOOT_LED_MODE_OFF && !led.logical_on && led.previous_ms == 0U);
}

static void test_first_packet_timeout(void) {
    const uint32_t starts[] = {100U, UINT32_MAX - 500U};
    for (unsigned index = 0U; index < 2U; ++index) {
        boot_context_t context = {.mode = BOOT_MODE_UPDATE_WINDOW, .update_started_ms = starts[index]};
        const uint32_t deadline = starts[index] + BOOT_UPDATE_FIRST_PACKET_TIMEOUT_MS;
        mock_upgrade_resets = mock_power_releases = 0U;
        assert(!boot_timeout_poll(&context, deadline - 1U));
        assert(mock_upgrade_resets == 0U);
        assert(boot_timeout_poll(&context, deadline));
        assert(mock_upgrade_resets == 1U && mock_power_releases == 0U);
        assert(!context.shutdown_pending);
        context.update_request_received = true;
        assert(!boot_timeout_poll(&context, deadline + 1U));
        assert(mock_upgrade_resets == 1U);
        context.update_request_received = false;
        context.mode = BOOT_MODE_RECOVERY;
        assert(!boot_timeout_poll(&context, deadline + 1U));
        context.mode = BOOT_MODE_START_APPLICATION;
        assert(!boot_timeout_poll(&context, deadline + 1U));
        assert(mock_upgrade_resets == 1U);
    }
}

static void test_shutdown_timing(void) {
    const uint32_t starts[] = {100U, UINT32_MAX - BOOT_UPDATE_WINDOW_MS - 500U,
                               UINT32_MAX - 2U * BOOT_UPDATE_WINDOW_MS - 500U};
    const boot_mode_t modes[] = {BOOT_MODE_RECOVERY, BOOT_MODE_UPDATE_WINDOW};
    for (unsigned index = 0U; index < 3U; ++index) {
        boot_context_t context = {
            .mode = modes[index % 2U], .update_started_ms = starts[index], .update_request_received = true};
        uint32_t deadline = context.update_started_ms + BOOT_UPDATE_WINDOW_MS;
        mock_led_writes = mock_power_releases = 0U;
        assert(!boot_timeout_poll(&context, deadline - 1U));
        assert(mock_led_writes == 0U && mock_power_releases == 0U);
        /* Successful update progress refreshes the idle deadline. */
        context.update_started_ms = deadline - 1U;
        assert(!boot_timeout_poll(&context, deadline));
        deadline = context.update_started_ms + BOOT_UPDATE_WINDOW_MS;
        assert(boot_timeout_poll(&context, deadline));
        assert(mock_led_mode == BOOT_LED_MODE_FATAL && mock_led_writes == 1U);
        assert(mock_power_releases == 0U);
        assert(boot_timeout_poll(&context, deadline + BOOT_SHUTDOWN_DELAY_MS - 1U));
        assert(mock_power_releases == 0U && mock_led_writes == 1U);
        assert(boot_timeout_poll(&context, deadline + BOOT_SHUTDOWN_DELAY_MS));
        assert(context.power_released && mock_power_releases == 1U && mock_led_writes == 2U);
        assert(boot_timeout_poll(&context, deadline + BOOT_SHUTDOWN_DELAY_MS + 10000U));
        assert(mock_power_releases == 1U && mock_led_writes == 2U);
    }
    boot_context_t app = {.mode = BOOT_MODE_START_APPLICATION};
    assert(!boot_timeout_poll(&app, BOOT_UPDATE_WINDOW_MS + 1000U));
    assert(!app.shutdown_pending);
}

int main(void) {
    test_first_packet_timeout();
    test_shutdown_timing();
    test_led_mode_timing();
    test_memory_ranges();
    test_vector_validation();
    test_mode_selection();
    puts("boot_core_tests: PASS");
    return 0;
}
