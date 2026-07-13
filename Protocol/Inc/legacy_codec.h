#ifndef LEGACY_CODEC_H
#define LEGACY_CODEC_H
#include "legacy_parser.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
bool legacy_codec_handshake_response(const legacy_frame_t* request, uint8_t* response,
                                     size_t capacity, size_t* response_length);
#endif
