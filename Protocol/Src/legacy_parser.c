#include "legacy_parser.h"
#include "legacy_crc16.h"
#include <string.h>

static void reset(legacy_parser_t* parser) {
    parser->length = 0U;
    parser->expected_length = 0U;
    parser->complete = false;
}

void legacy_parser_init(legacy_parser_t* parser) {
    if (parser != NULL) {
        memset(parser, 0, sizeof(*parser));
    }
}

static void push_sync(legacy_parser_t* parser, uint8_t byte) {
    static const uint8_t sync[3] = { 0xAAU, 0x44U, 0x18U };
    if (byte == sync[parser->length]) {
        parser->bytes[parser->length++] = byte;
    } else if (byte == 0xAAU) {
        parser->bytes[0] = byte;
        parser->length = 1U;
    } else {
        parser->length = 0U;
    }
}

legacy_parse_result_t legacy_parser_feed(legacy_parser_t* parser, const uint8_t* data, size_t length,
                                          uint32_t now_ms) {
    if ((parser == NULL) || ((data == NULL) && (length != 0U)) || parser->complete)
        return LEGACY_PARSE_INVALID;
    for (size_t index = 0U; index < length; ++index) {
        uint8_t byte = data[index];
        parser->last_activity_ms = now_ms;
        if (parser->length < 3U) {
            push_sync(parser, byte);
            continue;
        }
        if (parser->length >= LEGACY_MAX_FRAME_SIZE) {
            reset(parser);
            return LEGACY_PARSE_OVERFLOW;
        }
        parser->bytes[parser->length++] = byte;
        if (parser->length == LEGACY_FRAME_HEADER_SIZE) {
            uint8_t frame = parser->bytes[3];
            uint16_t payload = (uint16_t)parser->bytes[5] | ((uint16_t)parser->bytes[6] << 8U);
            if (parser->bytes[4] != (uint8_t)(frame ^ 0xFFU) || payload < LEGACY_INSTRUCTION_SIZE ||
                payload > LEGACY_MAX_PAYLOAD_SIZE) {
                reset(parser);
                return LEGACY_PARSE_INVALID;
            }
            parser->expected_length = (uint16_t)(LEGACY_FRAME_HEADER_SIZE + payload + LEGACY_FRAME_CRC_SIZE);
        }
        if ((parser->expected_length != 0U) && (parser->length == parser->expected_length)) {
            uint16_t payload = (uint16_t)(parser->expected_length - LEGACY_FRAME_HEADER_SIZE - LEGACY_FRAME_CRC_SIZE);
            uint16_t wire_crc = (uint16_t)parser->bytes[LEGACY_FRAME_HEADER_SIZE + payload] |
                                ((uint16_t)parser->bytes[LEGACY_FRAME_HEADER_SIZE + payload + 1U] << 8U);
            if (legacy_crc16_xmodem(&parser->bytes[LEGACY_FRAME_HEADER_SIZE], payload) != wire_crc) {
                reset(parser);
                return LEGACY_PARSE_INVALID;
            }
            parser->complete = true;
            return LEGACY_PARSE_FRAME;
        }
    }
    return LEGACY_PARSE_MORE;
}

void legacy_parser_on_timeout(legacy_parser_t* parser) {
    if (parser != NULL)
        reset(parser);
}

bool legacy_parser_take_frame(legacy_parser_t* parser, legacy_frame_t* frame) {
    if ((parser == NULL) || (frame == NULL) || !parser->complete)
        return false;
    memcpy(frame->bytes, parser->bytes, parser->length);
    frame->length = parser->length;
    reset(parser);
    return true;
}
