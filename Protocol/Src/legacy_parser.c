#include "legacy_parser.h"
#include <string.h>
void legacy_parser_reset(legacy_parser_t* parser) {
    if (parser != NULL)
        parser->length = 0U;
}
legacy_parse_result_t legacy_parser_push(legacy_parser_t* parser, const uint8_t* data, size_t length,
                                         bool transaction_end) {
    if ((parser == NULL) || ((data == NULL) && (length != 0U)))
        return LEGACY_PARSE_INVALID;
    if (length > (sizeof(parser->bytes) - parser->length)) {
        parser->length = 0U;
        return LEGACY_PARSE_OVERFLOW;
    }
    memcpy(&parser->bytes[parser->length], data, length);
    parser->length += length;
    if (parser->length >= 1U && parser->bytes[0] != 0xAAU) {
        parser->length = 0U;
        return LEGACY_PARSE_INVALID;
    }
    if (parser->length >= 2U && parser->bytes[1] != 0x44U) {
        parser->length = 0U;
        return LEGACY_PARSE_INVALID;
    }
    if (parser->length >= 3U && parser->bytes[2] != 0x18U) {
        parser->length = 0U;
        return LEGACY_PARSE_INVALID;
    }
    if (!transaction_end)
        return LEGACY_PARSE_MORE;
    return parser->length >= 3U ? LEGACY_PARSE_FRAME : LEGACY_PARSE_INVALID;
}
