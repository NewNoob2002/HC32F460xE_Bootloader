#include "legacy_codec.h"
#include "legacy_crc16.h"
#include <string.h>

bool legacy_codec_handshake_response(const legacy_frame_t* request, uint8_t* response,
                                     size_t capacity, size_t* response_length) {
    if ((request == NULL) || (response == NULL) || (response_length == NULL) ||
        (request->length < LEGACY_FRAME_HEADER_SIZE + LEGACY_INSTRUCTION_SIZE + LEGACY_FRAME_CRC_SIZE) ||
        (request->length > capacity) || (request->bytes[7] != 0x20U))
        return false;
    memcpy(response, request->bytes, request->length);
    response[8] = 0x00U;
    uint16_t payload = (uint16_t)response[5] | ((uint16_t)response[6] << 8U);
    uint16_t crc = legacy_crc16_xmodem(&response[LEGACY_FRAME_HEADER_SIZE], payload);
    response[LEGACY_FRAME_HEADER_SIZE + payload] = (uint8_t)crc;
    response[LEGACY_FRAME_HEADER_SIZE + payload + 1U] = (uint8_t)(crc >> 8U);
    *response_length = request->length;
    return true;
}
