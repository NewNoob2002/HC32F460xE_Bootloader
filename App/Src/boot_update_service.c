#include "boot_update_service.h"

#include <stddef.h>
#include <string.h>
#include "boot_config.h"
#include "boot_protocol_crc.h"
#include "bsp_i2c_slave.h"
#include "elog.h"

static boot_update_service_stats_t service_stats;
static uint8_t response_buffer[BOOT_PROTOCOL_MAX_FRAME_SIZE];

/**
 * @brief Encodes a legacy response frame using the request sequence number and payload.
 * @param frame Validated request frame.
 * @param payload_length Number of payload bytes to copy into the response.
 * @return Encoded response length.
 */
static uint16_t encode_response(const boot_protocol_frame_t* frame, uint16_t payload_length) {
    if (frame == NULL)
        return 0;

    uint16_t frame_id = frame->payload[PACKET_CMD_INDEX];
    // Implementation for encoding response
    return 0;
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
