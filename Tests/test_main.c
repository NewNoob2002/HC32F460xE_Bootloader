#include "app_validator.h"
#include "boot_memory_map.h"
#include "legacy_crc16.h"
#include "legacy_parser.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct { uint32_t base; uint32_t words[2]; } vectors_t;
static uint32_t mock_read(uint32_t address, void *context)
{
    vectors_t *vectors = context;
    return vectors->words[(address - vectors->base) / 4U];
}
static void test_crc(void)
{
    static const uint8_t text[] = "123456789";
    uint16_t crc = legacy_crc16_xmodem(text, sizeof(text) - 1U);
    assert(crc == 0x31C3U);
    assert((uint8_t)crc == 0xC3U);
    assert((uint8_t)(crc >> 8U) == 0x31U);
}
static void test_parser(void)
{
    legacy_parser_t parser;
    const uint8_t sync[] = {0xAAU, 0x44U, 0x18U};
    const uint8_t bad[] = {0xAAU, 0x45U};
    uint8_t large[LEGACY_PARSER_CAPACITY + 1U] = {0U};
    legacy_parser_reset(&parser);
    assert(legacy_parser_push(&parser, sync, 1U, false) == LEGACY_PARSE_MORE);
    assert(legacy_parser_push(&parser, &sync[1], 2U, true) == LEGACY_PARSE_FRAME);
    legacy_parser_reset(&parser);
    assert(legacy_parser_push(&parser, bad, sizeof(bad), true) == LEGACY_PARSE_INVALID);
    assert(parser.length == 0U);
    assert(legacy_parser_push(&parser, sync, 2U, true) == LEGACY_PARSE_INVALID);
    legacy_parser_reset(&parser);
    large[0] = 0xAAU; large[1] = 0x44U; large[2] = 0x18U;
    assert(legacy_parser_push(&parser, large, sizeof(large), true) == LEGACY_PARSE_OVERFLOW);
    assert(parser.length == 0U);
}
static void test_validator(void)
{
    vectors_t vectors = {APP_FLASH_BASE, {HC32_SRAM_BASE + 0x100U, APP_FLASH_BASE + 0x101U}};
    assert(app_validator_check(APP_FLASH_BASE, mock_read, &vectors));
    vectors.words[0] = HC32_SRAM_END;
    assert(app_validator_check(APP_FLASH_BASE, mock_read, &vectors));
    vectors.words[0] = UINT32_MAX; vectors.words[1] = UINT32_MAX;
    assert(!app_validator_check(APP_FLASH_BASE, mock_read, &vectors));
    vectors.words[0] = 0x10000000U; vectors.words[1] = APP_FLASH_BASE + 0x101U;
    assert(!app_validator_check(APP_FLASH_BASE, mock_read, &vectors));
    vectors.words[0] = HC32_SRAM_BASE + 0x100U; vectors.words[1] = BOOT_FLASH_BASE + 0x101U;
    assert(!app_validator_check(APP_FLASH_BASE, mock_read, &vectors));
    vectors.words[1] = APP_FLASH_BASE + 0x100U;
    assert(!app_validator_check(APP_FLASH_BASE, mock_read, &vectors));
    assert(!app_validator_check(BOOT_FLASH_BASE, mock_read, &vectors));
}
int main(void)
{
    test_crc(); test_parser(); test_validator();
    puts("boot_host_tests: PASS");
    return 0;
}
