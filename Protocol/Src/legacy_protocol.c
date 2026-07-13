#include "legacy_protocol.h"
#include "boot_config.h"
#include "boot_log.h"
#include "bsp_i2c_slave.h"
#include "legacy_crc16.h"
#include "legacy_codec.h"
#include "legacy_parser.h"

#include <string.h>

#define LEGACY_CMD_HANDSHAKE 0x20U
#define LEGACY_ACK_OK 0x00U

static legacy_parser_t parser;

void legacy_protocol_service_init(uint32_t now_ms) {
    legacy_parser_init(&parser);
    parser.last_activity_ms = now_ms;
}

static bool publish_handshake(const legacy_frame_t* request) {
    uint8_t response[LEGACY_MAX_FRAME_SIZE];
    size_t response_length = 0U;
    if (!legacy_codec_handshake_response(request, response, sizeof(response), &response_length))
        return false;
    return bsp_i2c_slave_publish_response(response, response_length);
}

void legacy_protocol_service_poll(uint32_t now_ms) {
    uint8_t input[BSP_I2C_RX_CAPACITY];
    size_t length = 0U;
    while (bsp_i2c_slave_take_rx_transaction(input, sizeof(input), &length)) {
        for (size_t index = 0U; index < length; ++index) {
            legacy_parse_result_t result = legacy_parser_feed(&parser, &input[index], 1U, now_ms);
            if (result == LEGACY_PARSE_FRAME) {
                legacy_frame_t frame;
                if (legacy_parser_take_frame(&parser, &frame)) {
                    BOOT_LOG_DEBUG("PROTO", "frame=%u cmd=0x%02x", (unsigned)frame.bytes[3], (unsigned)frame.bytes[7]);
                    if ((frame.bytes[7] == LEGACY_CMD_HANDSHAKE) && BOOT_ENABLE_LEGACY_HANDSHAKE) {
                        if (publish_handshake(&frame))
                            BOOT_LOG_INFO("OTA", "handshake accepted frame=%u", (unsigned)frame.bytes[3]);
                    }
                }
            }
        }
    }
    if ((parser.length != 0U) && ((uint32_t)(now_ms - parser.last_activity_ms) >= BOOT_PROTOCOL_PARTIAL_TIMEOUT_MS))
        legacy_parser_on_timeout(&parser);
}
