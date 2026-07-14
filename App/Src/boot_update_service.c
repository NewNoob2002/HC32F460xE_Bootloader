#include "boot_update_service.h"

#include <stddef.h>
#include <string.h>
#include "boot_config.h"
#include "boot_memory_map.h"
#include "boot_protocol_crc.h"
#include "bsp_flash.h"
#include "bsp_i2c_slave.h"
#include "bsp_status_led.h"

#if BOOT_CFG_LOGGING
#include "elog.h"
#else
#define log_i(...) ((void)0)
#define log_w(...) ((void)0)
#endif

static boot_update_service_stats_t service_stats;
static uint8_t response_buffer[BOOT_PROTOCOL_MAX_FRAME_SIZE];

/**
 * @brief Encodes a legacy response frame using the request sequence number and payload.
 * @param frame Validated request frame.
 * @param payload_length Number of payload bytes to copy into the response.
 * @return Encoded response length.
 */
static uint16_t encode_response(const boot_protocol_frame_t* frame, uint16_t payload_length) {
    uint16_t response_payload_length;
    uint16_t crc;
    uint32_t flash_address;
    uint8_t command;
    uint8_t result = PACKET_ACK_OK;

    if ((frame == NULL) || (payload_length < PACKET_INSTRUCT_SEGMENT_SIZE) || (payload_length > frame->payload_length)
        || (payload_length > BOOT_PROTOCOL_MAX_PAYLOAD_SIZE))
        return 0;

    command = frame->payload[0U];
    flash_address = (uint32_t)frame->payload[2U] | ((uint32_t)frame->payload[3U] << 8U)
                    | ((uint32_t)frame->payload[4U] << 16U) | ((uint32_t)frame->payload[5U] << 24U);

    switch (command) {
        case PACKET_CMD_HANDSHAKE:
            response_payload_length = payload_length;
            log_i("HANDSHAKE frame_number=%u", (unsigned int)frame->frame_number);
            break;

        case PACKET_CMD_ERASE_FLASH:
            response_payload_length = PACKET_INSTRUCT_SEGMENT_SIZE;
            if ((frame->payload[1U] != PACKET_CMD_TYPE_DATA) || (flash_address < APP_FLASH_BASE)
                || (flash_address >= APP_FLASH_END)) {
                result = PACKET_ACK_ADDR_ERROR;
            } else {
                log_i("ERASE_FLASH EFM pending address=0x%08lX", (unsigned long)flash_address);
            }
            break;

        case PACKET_CMD_APP_DOWNLOAD: {
            bsp_status_led_set_mode(BOOT_LED_MODE_UPDATE_WINDOW);
            const uint16_t data_length = (uint16_t)(payload_length - PACKET_INSTRUCT_SEGMENT_SIZE);
            response_payload_length = PACKET_INSTRUCT_SEGMENT_SIZE;
            if ((frame->payload[1U] != PACKET_CMD_TYPE_DATA) || (flash_address < APP_FLASH_BASE)
                || (flash_address >= APP_FLASH_END) || (data_length == 0U)
                || ((uint32_t)data_length > (APP_FLASH_END - flash_address))) {
                result = PACKET_ACK_ADDR_ERROR;
            } else {
                log_i("APP_DOWNLOAD EFM pending address=0x%08lX length=%u", (unsigned long)flash_address,
                      (unsigned int)data_length);
            }
            break;
        }

        case PACKET_CMD_JUMP_TO_APP:
            bsp_status_led_set_mode(BOOT_LED_MODE_OFF);
            response_payload_length = payload_length;
            log_i("JUMP_TO_APP EFM marker/jump pending");
            break;

        default:
            ++service_stats.unsupported_commands;
            log_w("Unsupported boot command=0x%02X", (unsigned int)command);
            return 0;
    }

    response_buffer[FRAME_HEAD_INDEX] = BOOT_MSG_SYN_BYTE1;
    response_buffer[FRAME_HEAD_INDEX + 1U] = BOOT_MSG_SYN_BYTE2;
    response_buffer[FRAME_HEAD_INDEX + 2U] = BOOT_MSG_SYN_BYTE3;
    response_buffer[FRAME_NUM_INDEX] = frame->frame_number;
    response_buffer[FRAME_XORNUM_INDEX] = (uint8_t)(frame->frame_number ^ FRAME_NUM_XOR_BYTE);
    response_buffer[FRAME_LENGTH_INDEX] = (uint8_t)response_payload_length;
    response_buffer[FRAME_LENGTH_INDEX + 1U] = (uint8_t)(response_payload_length >> 8U);
    memcpy(&response_buffer[FRAME_PACKET_INDEX], frame->payload, response_payload_length);
    response_buffer[PACKET_RESULT_INDEX] = result;

    crc = BootProtocolCrcCalculate(&response_buffer[FRAME_PACKET_INDEX], response_payload_length);
    response_buffer[FRAME_PACKET_INDEX + response_payload_length] = (uint8_t)crc;
    response_buffer[FRAME_PACKET_INDEX + response_payload_length + 1U] = (uint8_t)(crc >> 8U);
    return (uint16_t)(FRAME_HEADER_LEN + response_payload_length + BOOT_MSG_CRC_LEN);
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
    if (response_length == 0U)
        return;
    if (txBufferWrite(response_buffer, response_length) == (int)response_length)
        ++service_stats.responses_published;
    else
        ++service_stats.response_busy_drop;
}

void BootUpdateServiceFrameCallback(const boot_protocol_frame_t* frame, void* context) {
    (void)context;
    basp_i2c_slave_err_reset();
    BootUpdateServiceHandleFrame(frame);
}

void BootUpdateServiceGetStats(boot_update_service_stats_t* stats) {
    if (stats != NULL)
        *stats = service_stats;
}
