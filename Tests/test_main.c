#include "boot_app.h"
#include "legacy_codec.h"
#include "legacy_crc16.h"
#include "legacy_parser.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint8_t request[] = {
    0xAA,0x44,0x18,0x01,0xFE,0x0C,0x00,0x20,0x11,0,0,0,0,0,0,0,0,0,0,0xD4,0xE3
};
static const uint8_t response[] = {
    0xAA,0x44,0x18,0x01,0xFE,0x0C,0x00,0x20,0x00,0,0,0,0,0,0,0,0,0,0,0xA0,0x6E
};

static void test_crc_and_handshake(void) {
    assert(legacy_crc16_xmodem((const uint8_t*)"123456789", 9U) == 0x31C3U);
    legacy_parser_t parser;
    legacy_frame_t frame;
    legacy_parser_init(&parser);
    for (size_t i = 0U; i < sizeof(request); ++i)
        assert(legacy_parser_feed(&parser, &request[i], 1U, (uint32_t)i) ==
               (i + 1U == sizeof(request) ? LEGACY_PARSE_FRAME : LEGACY_PARSE_MORE));
    assert(legacy_parser_take_frame(&parser, &frame));
    uint8_t encoded[LEGACY_MAX_FRAME_SIZE];
    size_t length = 0U;
    assert(legacy_codec_handshake_response(&frame, encoded, sizeof(encoded), &length));
    assert(length == sizeof(response));
    assert(memcmp(encoded, response, sizeof(response)) == 0);
    assert(!legacy_codec_handshake_response(&frame, encoded, sizeof(response) - 1U, &length));
}

static void test_parser_rejection_and_timeout(void) {
    legacy_parser_t parser;
    legacy_parser_init(&parser);
    uint8_t bad[sizeof(request)];
    memcpy(bad, request, sizeof(bad));
    bad[4] = 0U;
    assert(legacy_parser_feed(&parser, bad, 7U, 0U) == LEGACY_PARSE_INVALID);
    legacy_parser_init(&parser);
    memcpy(bad, request, sizeof(bad));
    bad[5] = 11U;
    assert(legacy_parser_feed(&parser, bad, 7U, 0U) == LEGACY_PARSE_INVALID);
    legacy_parser_init(&parser);
    uint8_t garbage[] = { 0x00U, 0xAAU, 0xAAU, 0x44U, 0x18U };
    assert(legacy_parser_feed(&parser, garbage, sizeof(garbage), 1U) == LEGACY_PARSE_MORE);
    assert(parser.length == 3U);
    legacy_parser_on_timeout(&parser);
    assert(parser.length == 0U);
}

static void test_boot_modes(void) {
    boot_context_t context = {0};
    boot_app_select_mode(&context, false, false);
    assert(context.mode == BOOT_MODE_RECOVERY);
    boot_app_select_mode(&context, true, true);
    assert(context.mode == BOOT_MODE_UPDATE_WINDOW);
    boot_app_select_mode(&context, true, false);
    assert(context.mode == BOOT_MODE_START_APPLICATION);
}

int main(void) {
    test_crc_and_handshake();
    test_parser_rejection_and_timeout();
    test_boot_modes();
    puts("boot_host_tests: PASS");
    return 0;
}
