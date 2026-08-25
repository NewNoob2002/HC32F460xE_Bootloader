#include "boot_update_service.h"

#include <stddef.h>
#include <string.h>
#include "app_validator.h"
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
#if !BOOT_ENABLE_LEGACY_UPDATE_SIMULATION
static uint8_t verify_buffer[PACKET_DATA_SEGMENT_SIZE];
#endif
static bool erase_completed;
static bool jump_requested;
static bool arm_jump_after_publish;
static uint32_t session_programmed_bytes;
static uint32_t programmed_high_water;

#if BOOT_ENABLE_LEGACY_ERASE && BOOT_ENABLE_FLASH_UPDATE
static int32_t erase_application(void) {
#if BOOT_ENABLE_LEGACY_UPDATE_SIMULATION
    return BSP_FLASH_OK;
#else
    const uint32_t first_sector = APP_FLASH_BASE / BSP_FLASH_SECTOR_SIZE;
    const uint16_t sector_count = bsp_flash_sector_count(APP_FLASH_MAX_SIZE);

    for (uint16_t index = 0U; index < sector_count; ++index) {
        const int32_t result = bsp_flash_erase_sector(first_sector + index);
        if (result != BSP_FLASH_OK)
            return result;
    }
    return BSP_FLASH_OK;
#endif
}
#endif

#if BOOT_ENABLE_LEGACY_DOWNLOAD && BOOT_ENABLE_FLASH_UPDATE
static int32_t program_and_verify(uint32_t address, const uint8_t* data, uint16_t length) {
#if BOOT_ENABLE_LEGACY_UPDATE_SIMULATION
    (void)address;
    (void)data;
    (void)length;
    return BSP_FLASH_OK;
#else
    int32_t result = bsp_flash_write(address, data, length);

    if (result != BSP_FLASH_OK)
        return result;
    result = bsp_flash_read(address, verify_buffer, length);
    if (result != BSP_FLASH_OK)
        return result;
    return (memcmp(verify_buffer, data, length) == 0) ? BSP_FLASH_OK : -1;
#endif
}
#endif

/**
 * @brief Encodes a legacy response frame using the request sequence number and payload.
 * @param frame Validated request frame.
 * @param payload_length Number of payload bytes to copy into the response.
 * @return Encoded response length.
 */
static uint16_t encode_response(const boot_protocol_frame_t* frame, uint16_t payload_length) {
    uint16_t response_payload_length;
    uint16_t crc;
#if (BOOT_ENABLE_LEGACY_ERASE && BOOT_ENABLE_FLASH_UPDATE) || (BOOT_ENABLE_LEGACY_DOWNLOAD && BOOT_ENABLE_FLASH_UPDATE)
    uint32_t flash_address;
#endif
    uint8_t command;
    uint8_t result = PACKET_ACK_OK;

    if ((frame == NULL) || (payload_length < PACKET_INSTRUCT_SEGMENT_SIZE) || (payload_length > frame->payload_length)
        || (payload_length > BOOT_PROTOCOL_MAX_PAYLOAD_SIZE))
        return 0;

    command = frame->payload[0U];
#if (BOOT_ENABLE_LEGACY_ERASE && BOOT_ENABLE_FLASH_UPDATE) || (BOOT_ENABLE_LEGACY_DOWNLOAD && BOOT_ENABLE_FLASH_UPDATE)
    flash_address = (uint32_t)frame->payload[2U] | ((uint32_t)frame->payload[3U] << 8U)
                    | ((uint32_t)frame->payload[4U] << 16U) | ((uint32_t)frame->payload[5U] << 24U);
#endif

    switch (command) {
#if BOOT_ENABLE_LEGACY_HANDSHAKE
        case PACKET_CMD_HANDSHAKE:
            response_payload_length = payload_length;
            log_i("HANDSHAKE frame_number=%u", (unsigned int)frame->frame_number);
            break;
#endif

#if BOOT_ENABLE_LEGACY_ERASE && BOOT_ENABLE_FLASH_UPDATE
        case PACKET_CMD_ERASE_FLASH:
            response_payload_length = PACKET_INSTRUCT_SEGMENT_SIZE;
            if ((frame->payload[1U] != PACKET_CMD_TYPE_DATA) || (flash_address != APP_FLASH_BASE)) {
                result = PACKET_ACK_ADDR_ERROR;
            } else if (erase_completed && (session_programmed_bytes == 0U)) {
                log_i("ERASE_FLASH duplicate acknowledged");
            } else {
                erase_completed = false;
                jump_requested = false;
                if (erase_application() != BSP_FLASH_OK) {
                    ++service_stats.flash_errors;
                    result = PACKET_ACK_ERROR;
                    log_w("ERASE_FLASH failed");
                } else {
                    erase_completed = true;
                    session_programmed_bytes = 0U;
                    programmed_high_water = APP_FLASH_BASE;
                    ++service_stats.erase_commands;
                    log_i("ERASE_FLASH complete start=0x%08lX sectors=%u", (unsigned long)APP_FLASH_BASE,
                          (unsigned int)bsp_flash_sector_count(APP_FLASH_MAX_SIZE));
                }
            }
            break;
#endif

#if BOOT_ENABLE_LEGACY_DOWNLOAD && BOOT_ENABLE_FLASH_UPDATE
        case PACKET_CMD_APP_DOWNLOAD: {
            bsp_status_led_set_mode(BOOT_LED_MODE_UPDATE_WINDOW);
            const uint16_t data_length = (uint16_t)(payload_length - PACKET_INSTRUCT_SEGMENT_SIZE);
            response_payload_length = PACKET_INSTRUCT_SEGMENT_SIZE;
            if ((frame->payload[1U] != PACKET_CMD_TYPE_DATA) || (flash_address < APP_FLASH_BASE)
                || (flash_address >= APP_FLASH_END) || (data_length == 0U)
                || ((uint32_t)data_length > (APP_FLASH_END - flash_address))) {
                result = PACKET_ACK_ADDR_ERROR;
            } else if (!erase_completed) {
                result = PACKET_ACK_ERROR;
            } else if (program_and_verify(flash_address, &frame->payload[PACKET_INSTRUCT_SEGMENT_SIZE], data_length)
                       != BSP_FLASH_OK) {
                ++service_stats.flash_errors;
                result = PACKET_ACK_ERROR;
                log_w("APP_DOWNLOAD failed address=0x%08lX length=%u", (unsigned long)flash_address,
                      (unsigned int)data_length);
            } else {
                service_stats.programmed_bytes += data_length;
                session_programmed_bytes += data_length;
                if ((flash_address + data_length) > programmed_high_water)
                    programmed_high_water = flash_address + data_length;
                log_i("APP_DOWNLOAD complete address=0x%08lX length=%u", (unsigned long)flash_address,
                      (unsigned int)data_length);
            }
            break;
        }
#endif

#if BOOT_ENABLE_LEGACY_JUMP && BOOT_ENABLE_APPLICATION_JUMP
        case PACKET_CMD_JUMP_TO_APP:
            response_payload_length = payload_length;
            if (!erase_completed || !boot_application_vector_is_valid()) {
                result = PACKET_ACK_ERROR;
                log_w("JUMP_TO_APP rejected erased=%u bytes=%lu high=0x%08lX", erase_completed ? 1U : 0U,
                      (unsigned long)session_programmed_bytes, (unsigned long)programmed_high_water);
            } else {
                bsp_status_led_set_mode(BOOT_LED_MODE_OFF);
                arm_jump_after_publish = true;
                log_i("JUMP_TO_APP accepted; waiting for ACK transmission");
            }
            break;
#endif

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
#if !BOOT_ENABLE_LEGACY_UPDATE_SIMULATION
    memset(verify_buffer, 0, sizeof(verify_buffer));
#endif
    erase_completed = false;
    jump_requested = false;
    arm_jump_after_publish = false;
    session_programmed_bytes = 0U;
    programmed_high_water = APP_FLASH_BASE;
}

void BootUpdateServiceHandleFrame(const boot_protocol_frame_t* frame) {
    uint16_t response_length;

    if (frame == NULL)
        return;
    ++service_stats.validated_frames;

    if (!txBufferReserve()) {
        ++service_stats.response_busy_drop;
        return;
    }

    arm_jump_after_publish = false;
    response_length = encode_response(frame, frame->payload_length);
    if (response_length == 0U) {
        txBufferCancelWrite();
        return;
    }
    if (txBufferWrite(response_buffer, response_length) == (int)response_length) {
        ++service_stats.responses_published;
        if (arm_jump_after_publish) {
            jump_requested = true;
            log_i("JUMP ACK queued length=%u", (unsigned int)response_length);
        }
    } else {
        ++service_stats.response_busy_drop;
    }
    arm_jump_after_publish = false;
}

void BootUpdateServiceFrameCallback(const boot_protocol_frame_t* frame, void* context) {
    (void)context;
    BootUpdateServiceHandleFrame(frame);
}

void BootUpdateServiceGetStats(boot_update_service_stats_t* stats) {
    if (stats != NULL)
        *stats = service_stats;
}

bool BootUpdateServiceTakeJumpRequest(void) {
    if (!jump_requested || (bsp_i2c_slave_get_state() != SLAVE_TX_DONE))
        return false;
    const bool requested = jump_requested;
    jump_requested = false;
    return requested;
}
