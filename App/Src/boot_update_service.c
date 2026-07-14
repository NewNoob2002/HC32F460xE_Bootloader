#include "boot_update_service.h"

#include <stddef.h>
#include <string.h>
#include "boot_config.h"
#include "boot_protocol_crc.h"
#include "bsp_i2c_slave.h"
#include "elog.h"

#define BOOT_COMMAND_HANDSHAKE     0x20U
#define BOOT_PAYLOAD_COMMAND_INDEX 0U
#define BOOT_PAYLOAD_RESULT_INDEX  1U
#define BOOT_ACK_OK                0x00U

static boot_update_service_stats_t service_stats;
static uint8_t response_buffer[BOOT_PROTOCOL_MAX_FRAME_SIZE];

/**
 * @brief Encodes a legacy response frame using the request sequence number and payload.
 * @param frame Validated request frame.
 * @param payload_length Number of payload bytes to copy into the response.
 * @return Encoded response length.
 */
static uint16_t encode_response(const boot_protocol_frame_t* frame, uint16_t payload_length) {
    uint16_t crc;

    response_buffer[0] = BOOT_PROTOCOL_SYNC_1;
    response_buffer[1] = BOOT_PROTOCOL_SYNC_2;
    response_buffer[2] = BOOT_PROTOCOL_SYNC_3;
    response_buffer[3] = frame->frame_number;
    response_buffer[4] = (uint8_t)(frame->frame_number ^ 0xFFU);
    response_buffer[5] = (uint8_t)payload_length;
    response_buffer[6] = (uint8_t)(payload_length >> 8U);
    memcpy(&response_buffer[BOOT_PROTOCOL_HEADER_SIZE], frame->payload, payload_length);
    response_buffer[BOOT_PROTOCOL_HEADER_SIZE + BOOT_PAYLOAD_RESULT_INDEX] = BOOT_ACK_OK;
    crc = BootProtocolCrcCalculate(&response_buffer[BOOT_PROTOCOL_HEADER_SIZE], payload_length);
    response_buffer[BOOT_PROTOCOL_HEADER_SIZE + payload_length] = (uint8_t)crc;
    response_buffer[BOOT_PROTOCOL_HEADER_SIZE + payload_length + 1U] = (uint8_t)(crc >> 8U);
    return (uint16_t)(BOOT_PROTOCOL_HEADER_SIZE + payload_length + BOOT_PROTOCOL_CRC_SIZE);
}

void BootUpdateServiceInit(void) {
    memset(&service_stats, 0, sizeof(service_stats));
    memset(response_buffer, 0, sizeof(response_buffer));
}

void BootUpdateServiceHandleFrame(const boot_protocol_frame_t* frame) {
    uint16_t response_length;

    if (frame == NULL)
        return;
    ++service_stats.validated_frames;

    if ((frame->payload[BOOT_PAYLOAD_COMMAND_INDEX] != BOOT_COMMAND_HANDSHAKE) || (BOOT_ENABLE_LEGACY_HANDSHAKE == 0)) {
        ++service_stats.unsupported_commands;
        return;
    }
    if (txBufferAvailable() != 0) {
        ++service_stats.response_busy_drop;
        return;
    }
    response_length = encode_response(frame, frame->payload_length);
    if (txBufferWrite(response_buffer, response_length) == (int)response_length)
        ++service_stats.responses_published;
    else
        ++service_stats.response_busy_drop;
}

void BootUpdateServiceFrameCallback(const boot_protocol_frame_t* frame, void* context) {
    (void)context;
    log_i("BootUpdateServiceFrameCallback frame_number=%u payload_length=%u", frame->frame_number,
          frame->payload_length);
    basp_i2c_slave_err_reset();
    BootUpdateServiceHandleFrame(frame);
}

void BootUpdateServiceGetStats(boot_update_service_stats_t* stats) {
    if (stats != NULL)
        *stats = service_stats;
}
