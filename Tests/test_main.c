#include "app_validator.h"
#include "boot_memory_map.h"
#include "legacy_crc16.h"
#include "legacy_parser.h"
#include <assert.h>
#include <stdio.h>
typedef struct { uint32_t base; uint32_t words[2]; } vectors_t;
static uint32_t mock_read(uint32_t address, void *context)
{
    vectors_t *vectors = context;
    return vectors->words[(address - vectors->base) / 4U];
}
void test_boot_time(void); void test_status_led(void); void test_external_watchdog(void); void test_power_policy(void);
int main(void)
{
    static const uint8_t text[] = "123456789";
    legacy_parser_t parser;
    const uint8_t sync[] = {0xAAU, 0x44U, 0x18U};
    vectors_t vectors = {APP_FLASH_BASE, {HC32_SRAM_END, APP_FLASH_BASE + 0x101U}};
    assert(legacy_crc16_xmodem(text, sizeof(text) - 1U) == 0x31C3U);
    legacy_parser_reset(&parser);
    assert(legacy_parser_push(&parser, sync, 1U, false) == LEGACY_PARSE_MORE);
    assert(legacy_parser_push(&parser, &sync[1], 2U, true) == LEGACY_PARSE_FRAME);
    assert(app_validator_check(APP_FLASH_BASE, mock_read, &vectors));
    test_boot_time(); test_status_led(); test_external_watchdog(); test_power_policy();
    puts("boot_host_tests: PASS");
    return 0;
}
