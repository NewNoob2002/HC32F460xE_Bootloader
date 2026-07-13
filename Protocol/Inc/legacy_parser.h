#ifndef LEGACY_PARSER_H
#define LEGACY_PARSER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define LEGACY_PARSER_CAPACITY 528U
typedef enum { LEGACY_PARSE_MORE, LEGACY_PARSE_FRAME, LEGACY_PARSE_INVALID, LEGACY_PARSE_OVERFLOW } legacy_parse_result_t;
typedef struct { uint8_t bytes[LEGACY_PARSER_CAPACITY]; size_t length; } legacy_parser_t;
void legacy_parser_reset(legacy_parser_t *parser);
legacy_parse_result_t legacy_parser_push(legacy_parser_t *parser, const uint8_t *data, size_t length, bool transaction_end);
#endif
