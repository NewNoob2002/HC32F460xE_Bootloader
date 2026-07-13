#ifndef LEGACY_PARSER_H
#define LEGACY_PARSER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LEGACY_INSTRUCTION_SIZE 12U
#define LEGACY_MAX_DATA_SIZE 512U
#define LEGACY_MAX_PAYLOAD_SIZE 524U
#define LEGACY_FRAME_HEADER_SIZE 7U
#define LEGACY_FRAME_CRC_SIZE 2U
#define LEGACY_MAX_FRAME_SIZE 533U

typedef enum {
    LEGACY_PARSE_MORE,
    LEGACY_PARSE_FRAME,
    LEGACY_PARSE_INVALID,
    LEGACY_PARSE_OVERFLOW
} legacy_parse_result_t;

typedef struct {
    uint8_t bytes[LEGACY_MAX_FRAME_SIZE];
    uint16_t length;
} legacy_frame_t;

typedef struct {
    uint8_t bytes[LEGACY_MAX_FRAME_SIZE];
    uint16_t length;
    uint16_t expected_length;
    uint32_t last_activity_ms;
    bool complete;
} legacy_parser_t;

void legacy_parser_init(legacy_parser_t* parser);
legacy_parse_result_t legacy_parser_feed(legacy_parser_t* parser, const uint8_t* data, size_t length,
                                          uint32_t now_ms);
void legacy_parser_on_timeout(legacy_parser_t* parser);
bool legacy_parser_take_frame(legacy_parser_t* parser, legacy_frame_t* frame);
#endif
