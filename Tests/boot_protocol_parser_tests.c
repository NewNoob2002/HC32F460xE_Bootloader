#include "boot_config.h"
#include "boot_memory_map.h"
#include "boot_protocol_crc.h"
#include "boot_protocol_parser.h"
#include "boot_status_led.h"
#include "boot_update_service.h"
#include "bsp_flash.h"
#include "bsp_i2c_slave.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifndef BOOT_TEST_UPDATE_MODE
#define BOOT_TEST_UPDATE_MODE 1
#endif

static uint8_t mock_tx[BSP_I2C_TX_CAPACITY];
static uint16_t mock_tx_length;
static uint32_t mock_now_ms;
static uint32_t mock_complete_reads;
static bool mock_tx_pending;
static bool mock_tx_reserved;
static i2c_slave_state_t mock_i2c_state = SLAVE_RX_DONE;
static boot_protocol_frame_t callback_frame;
static uint32_t callback_count;
static uint32_t mock_erase_count;
static uint32_t mock_write_address;
static uint32_t mock_write_length;
static uint8_t mock_write_data[BOOT_PROTOCOL_MAX_DATA_SIZE];
static int32_t mock_flash_result;
static bool mock_app_valid;
static boot_led_mode_t mock_led_mode;

void boot_protocol_crc_tests_run(void);

int32_t bsp_flash_erase_sector(uint32_t sector) {
    assert(sector == (APP_FLASH_BASE / BSP_FLASH_SECTOR_SIZE) + mock_erase_count);
    ++mock_erase_count;
    return mock_flash_result;
}

int32_t bsp_flash_write(uint32_t address, const uint8_t* data, uint32_t length) {
    mock_write_address = address;
    mock_write_length = length;
    memcpy(mock_write_data, data, length);
    return mock_flash_result;
}

int32_t bsp_flash_read(uint32_t address, uint8_t* data, uint32_t length) {
    assert(address == mock_write_address);
    assert(length == mock_write_length);
    memcpy(data, mock_write_data, length);
    return mock_flash_result;
}

uint16_t bsp_flash_sector_count(uint32_t size) {
    return (uint16_t)((size + BSP_FLASH_SECTOR_SIZE - 1U) / BSP_FLASH_SECTOR_SIZE);
}

bool boot_application_vector_is_valid(void) {
    return mock_app_valid;
}

void boot_status_led_set_mode(boot_led_mode_t mode) {
    mock_led_mode = mode;
}

i2c_slave_state_t bsp_i2c_slave_get_state(void) {
    return mock_i2c_state;
}

uint32_t bsp_time_ms(void) {
    return mock_now_ms;
}

void bsp_i2c_slave_get_counters(bsp_i2c_slave_counters_t* counters) {
    memset(counters, 0, sizeof(*counters));
    counters->tx_complete_reads = mock_complete_reads;
}

int tx_buffer_available(void) {
    return mock_tx_pending ? (int)mock_tx_length : 0;
}

bool tx_buffer_reserve(void) {
    if (mock_tx_pending || mock_tx_reserved || (mock_i2c_state == SLAVE_TX))
        return false;
    mock_tx_reserved = true;
    return true;
}

void tx_buffer_cancel_write(void) {
    mock_tx_reserved = false;
}

int tx_buffer_write(const uint8_t* buffer, uint16_t length) {
    assert(buffer != NULL);
    assert(length <= sizeof(mock_tx));
    assert(mock_tx_reserved);
    memcpy(mock_tx, buffer, length);
    mock_tx_length = length;
    mock_tx_pending = true;
    mock_tx_reserved = false;
    mock_i2c_state = SLAVE_RX;
    return (int)length;
}

/** @brief Captures a validated parser frame before its internal storage is reused. */
static void capture_frame(const boot_protocol_frame_t* frame, void* context) {
    (void)context;
    callback_frame = *frame;
    ++callback_count;
}

/** @brief Encodes a valid frame using the production CRC implementation. */
static size_t make_frame(uint8_t* output, uint8_t frame_number, const uint8_t* payload, uint16_t payload_length) {
    const uint16_t crc = boot_protocol_crc_calculate(payload, payload_length);
    const size_t frame_length = BOOT_PROTOCOL_HEADER_SIZE + payload_length + BOOT_PROTOCOL_CRC_SIZE;

    output[0] = BOOT_PROTOCOL_SYNC_1;
    output[1] = BOOT_PROTOCOL_SYNC_2;
    output[2] = BOOT_PROTOCOL_SYNC_3;
    output[3] = frame_number;
    output[4] = (uint8_t)(frame_number ^ 0xFFU);
    output[5] = (uint8_t)payload_length;
    output[6] = (uint8_t)(payload_length >> 8U);
    memcpy(&output[BOOT_PROTOCOL_HEADER_SIZE], payload, payload_length);
    output[BOOT_PROTOCOL_HEADER_SIZE + payload_length] = (uint8_t)crc;
    output[BOOT_PROTOCOL_HEADER_SIZE + payload_length + 1U] = (uint8_t)(crc >> 8U);
    return frame_length;
}

/** @brief Feeds bytes individually and returns the valid-frame completion count. */
static uint32_t feed_bytes(boot_protocol_parser_t* parser, const uint8_t* bytes, size_t length) {
    return (uint32_t)boot_protocol_parser_push_bytes(parser, bytes, length);
}

/** @brief Verifies chunk input preserves state and counts every completed frame. */
static void test_push_bytes(void) {
    boot_protocol_parser_t parser;
    uint8_t payload[BOOT_PROTOCOL_MIN_PAYLOAD_SIZE] = {PACKET_CMD_HANDSHAKE, PACKET_CMD_TYPE_DATA};
    uint8_t first[BOOT_PROTOCOL_MAX_FRAME_SIZE];
    uint8_t second[BOOT_PROTOCOL_MAX_FRAME_SIZE];
    uint8_t stream[BOOT_PROTOCOL_MAX_FRAME_SIZE * 2U + 3U];
    const size_t first_length = make_frame(first, 0x21U, payload, sizeof(payload));
    const size_t second_length = make_frame(second, 0x22U, payload, sizeof(payload));

    boot_protocol_parser_init(&parser);
    boot_protocol_parser_register_callback(&parser, capture_frame, NULL);
    callback_count = 0U;
    assert(boot_protocol_parser_push_bytes(NULL, first, first_length) == 0U);
    assert(boot_protocol_parser_push_bytes(&parser, NULL, first_length) == 0U);
    assert(boot_protocol_parser_push_bytes(&parser, first, 0U) == 0U);

    assert(boot_protocol_parser_push_bytes(&parser, first, 3U) == 0U);
    assert(boot_protocol_parser_has_partial_frame(&parser));
    assert(boot_protocol_parser_push_bytes(&parser, &first[3U], first_length - 3U) == 1U);
    assert(callback_count == 1U);
    assert(callback_frame.frame_number == 0x21U);

    stream[0] = 0x55U;
    stream[1] = 0xAAU;
    stream[2] = 0x00U;
    memcpy(&stream[3U], first, first_length);
    memcpy(&stream[3U + first_length], second, second_length);
    assert(boot_protocol_parser_push_bytes(&parser, stream, 3U + first_length + second_length) == 2U);
    assert(callback_count == 3U);
    assert(callback_frame.frame_number == 0x22U);
}

/** @brief Checks the common response header, result byte, and payload CRC. */
static void assert_response(uint8_t frame_number, uint8_t command, uint8_t result, uint16_t payload_length) {
    const uint16_t crc = boot_protocol_crc_calculate(&mock_tx[BOOT_PROTOCOL_HEADER_SIZE], payload_length);

    assert(mock_tx_length == BOOT_PROTOCOL_HEADER_SIZE + payload_length + BOOT_PROTOCOL_CRC_SIZE);
    assert(mock_tx[0] == BOOT_PROTOCOL_SYNC_1);
    assert(mock_tx[1] == BOOT_PROTOCOL_SYNC_2);
    assert(mock_tx[2] == BOOT_PROTOCOL_SYNC_3);
    assert(mock_tx[3] == frame_number);
    assert(mock_tx[4] == (uint8_t)(frame_number ^ 0xFFU));
    assert(mock_tx[5] == (uint8_t)payload_length);
    assert(mock_tx[6] == (uint8_t)(payload_length >> 8U));
    assert(mock_tx[7] == command);
    assert(mock_tx[8] == result);
    assert(mock_tx[BOOT_PROTOCOL_HEADER_SIZE + payload_length] == (uint8_t)crc);
    assert(mock_tx[BOOT_PROTOCOL_HEADER_SIZE + payload_length + 1U] == (uint8_t)(crc >> 8U));
    mock_tx_pending = false;
    mock_i2c_state = SLAVE_TX_DONE;
    ++mock_complete_reads;
}

/** @brief Verifies synchronization recovery, callback content, and minimum payload framing. */
static void test_sync_and_callback(void) {
    boot_protocol_parser_t parser;
    uint8_t payload[BOOT_PROTOCOL_MIN_PAYLOAD_SIZE] = {0x20U, 0x11U};
    uint8_t frame[BOOT_PROTOCOL_MAX_FRAME_SIZE];
    uint8_t stream[BOOT_PROTOCOL_MAX_FRAME_SIZE + 9U];
    static const uint8_t noise[] = {0x55U, 0xAAU, 0x44U, 0x00U, 0xAAU, 0xAAU, 0xAAU};
    const size_t length = make_frame(frame, 7U, payload, sizeof(payload));

    boot_protocol_parser_init(&parser);
    boot_protocol_parser_register_callback(&parser, capture_frame, NULL);
    callback_count = 0U;
    memcpy(stream, noise, sizeof(noise));
    memcpy(&stream[sizeof(noise)], frame, length);
    assert(feed_bytes(&parser, stream, sizeof(noise) + length) == 1U);
    assert(callback_count == 1U);
    assert(callback_frame.frame_number == 7U);
    assert(callback_frame.payload_length == sizeof(payload));
    assert(memcmp(callback_frame.payload, payload, sizeof(payload)) == 0);
}

/** @brief Verifies invalid headers and CRCs are rejected before callback dispatch. */
static void test_rejection_and_recovery(void) {
    boot_protocol_parser_t parser;
    boot_protocol_parser_stats_t stats;
    uint8_t payload[BOOT_PROTOCOL_MIN_PAYLOAD_SIZE] = {0};
    uint8_t frame[BOOT_PROTOCOL_MAX_FRAME_SIZE];
    uint8_t invalid[BOOT_PROTOCOL_MAX_FRAME_SIZE];
    const size_t length = make_frame(frame, 3U, payload, sizeof(payload));

    boot_protocol_parser_init(&parser);
    boot_protocol_parser_register_callback(&parser, capture_frame, NULL);
    callback_count = 0U;
    memcpy(invalid, frame, length);
    invalid[4] = 0U;
    assert(feed_bytes(&parser, invalid, BOOT_PROTOCOL_HEADER_SIZE) == 0U);
    memcpy(invalid, frame, length);
    invalid[5] = 0U;
    invalid[6] = 0U;
    assert(feed_bytes(&parser, invalid, BOOT_PROTOCOL_HEADER_SIZE) == 0U);
    memcpy(invalid, frame, length);
    invalid[5] = (uint8_t)(BOOT_PROTOCOL_MIN_PAYLOAD_SIZE - 1U);
    invalid[6] = 0U;
    assert(feed_bytes(&parser, invalid, BOOT_PROTOCOL_HEADER_SIZE) == 0U);
    memcpy(invalid, frame, length);
    invalid[5] = (uint8_t)(BOOT_PROTOCOL_MAX_PAYLOAD_SIZE + 1U);
    invalid[6] = (uint8_t)((BOOT_PROTOCOL_MAX_PAYLOAD_SIZE + 1U) >> 8U);
    assert(feed_bytes(&parser, invalid, BOOT_PROTOCOL_HEADER_SIZE) == 0U);
    memcpy(invalid, frame, length);
    invalid[BOOT_PROTOCOL_HEADER_SIZE + 2U] ^= 0x80U;
    assert(feed_bytes(&parser, invalid, length) == 0U);
    memcpy(invalid, frame, length);
    invalid[length - 1U] ^= 1U;
    assert(feed_bytes(&parser, invalid, length) == 0U);
    assert(callback_count == 0U);
    assert(feed_bytes(&parser, frame, length) == 1U);
    boot_protocol_parser_get_stats(&parser, &stats);
    assert(stats.length_error == 3U);
    assert(stats.crc_error == 2U);
    assert(callback_count == 1U);
}

/** @brief Verifies the maximum frame and caller-managed timeout recovery. */
static void test_maximum_and_timeout(void) {
    boot_protocol_parser_t parser;
    uint8_t payload[BOOT_PROTOCOL_MAX_PAYLOAD_SIZE];
    uint8_t frame[BOOT_PROTOCOL_MAX_FRAME_SIZE];
    for (size_t index = 0U; index < sizeof(payload); ++index)
        payload[index] = (uint8_t)index;
    const size_t length = make_frame(frame, 9U, payload, sizeof(payload));

    assert(length == BOOT_PROTOCOL_MAX_FRAME_SIZE);
    boot_protocol_parser_init(&parser);
    boot_protocol_parser_register_callback(&parser, capture_frame, NULL);
    assert(feed_bytes(&parser, frame, 8U) == 0U);
    assert(boot_protocol_parser_has_partial_frame(&parser));
    boot_protocol_parser_timeout(&parser);
    assert(!boot_protocol_parser_has_partial_frame(&parser));
    boot_protocol_parser_stats_t stats;
    boot_protocol_parser_get_stats(&parser, &stats);
    assert(stats.timeout == 1U);
    assert(feed_bytes(&parser, frame, length) == 1U);
    assert(feed_bytes(&parser, frame, length) == 1U);
}

/** @brief Verifies chunk input produces the legacy handshake response through tx_buffer_write. */
static void test_chunk_and_legacy_callback(void) {
    static const uint8_t request[] = {0xAAU, 0x44U, 0x18U, 0x01U, 0xFEU, 0x0CU, 0x00U, 0x20U, 0x11U, 0U,   0U,
                                      0U,    0U,    0U,    0U,    0U,    0U,    0U,    0U,    0xD4U, 0xE3U};
    static const uint8_t response[] = {0xAAU, 0x44U, 0x18U, 0x01U, 0xFEU, 0x0CU, 0x00U, 0x20U, 0x00U, 0U,   0U,
                                       0U,    0U,    0U,    0U,    0U,    0U,    0U,    0U,    0xA0U, 0x6EU};
    boot_protocol_parser_t parser;
    boot_update_service_stats_t stats;

    mock_tx_length = 0U;
    mock_tx_pending = false;
    mock_tx_reserved = false;
    mock_i2c_state = SLAVE_RX_DONE;
    boot_update_service_init();
    boot_protocol_parser_init(&parser);
    boot_protocol_parser_register_callback(&parser, boot_update_service_frame_callback, NULL);
    assert(boot_protocol_parser_push_bytes(&parser, request, sizeof(request)) == 1U);
    assert(mock_tx_length == sizeof(response));
    assert(memcmp(mock_tx, response, sizeof(response)) == 0);
    assert_response(1U, PACKET_CMD_HANDSHAKE, PACKET_ACK_OK, BOOT_PROTOCOL_MIN_PAYLOAD_SIZE);
    boot_update_service_get_stats(&stats);
    assert(stats.validated_frames == 1U);
    assert(stats.responses_published == 1U);
}

static void test_first_update_request(void) {
    boot_protocol_parser_t parser;
    uint8_t payload[BOOT_PROTOCOL_MIN_PAYLOAD_SIZE] = {PACKET_CMD_HANDSHAKE, PACKET_CMD_TYPE_CONTROL};
    uint8_t frame[BOOT_PROTOCOL_MAX_FRAME_SIZE];
    const size_t length = make_frame(frame, 1U, payload, sizeof(payload));

    boot_update_service_init();
    boot_protocol_parser_init(&parser);
    boot_protocol_parser_register_callback(&parser, boot_update_service_frame_callback, NULL);
    assert(!boot_update_service_has_request());
    frame[length - 1U] ^= 1U;
    assert(feed_bytes(&parser, frame, length) == 0U);
    assert(!boot_update_service_has_request());

    payload[0] = PACKET_CMD_APP_UPLOAD; /* Valid CRC, unsupported command. */
    assert(feed_bytes(&parser, frame, make_frame(frame, 2U, payload, sizeof(payload))) == 1U);
    assert(!boot_update_service_has_request());

    payload[0] = PACKET_CMD_HANDSHAKE;
    mock_tx_pending = true; /* A valid request still counts when the response is busy. */
    assert(feed_bytes(&parser, frame, make_frame(frame, 3U, payload, sizeof(payload))) == 1U);
    assert(boot_update_service_has_request());
    mock_tx_pending = false;
}

#if BOOT_TEST_UPDATE_MODE != 0
static void test_update_led_and_progress(void) {
    boot_protocol_frame_t frame = {0};
    frame.payload_length = BOOT_PROTOCOL_MIN_PAYLOAD_SIZE + 4U;
    frame.payload[0] = PACKET_CMD_APP_DOWNLOAD;
    frame.payload[1] = PACKET_CMD_TYPE_DATA;
    frame.payload[3] = 0x80U;
    mock_tx_pending = mock_tx_reserved = false;
    mock_i2c_state = SLAVE_RX_DONE;
    mock_flash_result = BSP_FLASH_OK;
    mock_erase_count = 0U;
    mock_led_mode = BOOT_LED_MODE_RECOVERY;
    boot_update_service_init();
    assert(!boot_update_service_take_progress());
    boot_update_service_handle_frame(&frame); /* Download before erase is rejected. */
    assert_response(0U, PACKET_CMD_APP_DOWNLOAD, PACKET_ACK_ERROR, BOOT_PROTOCOL_MIN_PAYLOAD_SIZE);
    assert(mock_led_mode == BOOT_LED_MODE_RECOVERY && !boot_update_service_take_progress());

    frame.payload[1] = PACKET_CMD_TYPE_CONTROL;
    boot_update_service_handle_frame(&frame);
    assert_response(0U, PACKET_CMD_APP_DOWNLOAD, PACKET_ACK_ADDR_ERROR, BOOT_PROTOCOL_MIN_PAYLOAD_SIZE);
    assert(mock_led_mode == BOOT_LED_MODE_RECOVERY && !boot_update_service_take_progress());
    frame.payload[1] = PACKET_CMD_TYPE_DATA;
    frame.payload[0] = PACKET_CMD_ERASE_FLASH;
    boot_update_service_handle_frame(&frame);
    assert_response(0U, PACKET_CMD_ERASE_FLASH, PACKET_ACK_OK, BOOT_PROTOCOL_MIN_PAYLOAD_SIZE);
    assert(mock_led_mode == BOOT_LED_MODE_UPDATE_WINDOW && boot_update_service_take_progress());
    assert(!boot_update_service_take_progress());

    frame.payload[0] = PACKET_CMD_APP_DOWNLOAD;
    frame.payload[3] = 0U;
    boot_update_service_handle_frame(&frame);
    assert_response(0U, PACKET_CMD_APP_DOWNLOAD, PACKET_ACK_ADDR_ERROR, BOOT_PROTOCOL_MIN_PAYLOAD_SIZE);
    assert(mock_led_mode == BOOT_LED_MODE_UPDATE_WINDOW && !boot_update_service_take_progress());
    frame.payload[3] = 0x80U;
#if BOOT_TEST_UPDATE_MODE == 1
    mock_flash_result = -1;
    boot_update_service_handle_frame(&frame);
    assert_response(0U, PACKET_CMD_APP_DOWNLOAD, PACKET_ACK_ERROR, BOOT_PROTOCOL_MIN_PAYLOAD_SIZE);
    assert(mock_led_mode == BOOT_LED_MODE_FATAL && !boot_update_service_take_progress());
    mock_flash_result = BSP_FLASH_OK;
#endif
    boot_update_service_handle_frame(&frame);
    assert_response(0U, PACKET_CMD_APP_DOWNLOAD, PACKET_ACK_OK, BOOT_PROTOCOL_MIN_PAYLOAD_SIZE);
    assert(mock_led_mode == BOOT_LED_MODE_UPDATE_WINDOW && boot_update_service_take_progress());

    frame.payload[0] = PACKET_CMD_JUMP_TO_APP;
    mock_app_valid = false;
    boot_update_service_handle_frame(&frame);
    assert_response(0U, PACKET_CMD_JUMP_TO_APP, PACKET_ACK_ERROR, frame.payload_length);
    assert(mock_led_mode == BOOT_LED_MODE_FATAL && !boot_update_service_take_progress());
    frame.payload[0] = PACKET_CMD_HANDSHAKE;
    boot_update_service_handle_frame(&frame);
    assert_response(0U, PACKET_CMD_HANDSHAKE, PACKET_ACK_OK, frame.payload_length);
    assert(mock_led_mode == BOOT_LED_MODE_FATAL && !boot_update_service_take_progress());
    frame.payload[0] = PACKET_CMD_JUMP_TO_APP;
    mock_app_valid = true;
    boot_update_service_handle_frame(&frame);
    assert_response(0U, PACKET_CMD_JUMP_TO_APP, PACKET_ACK_OK, frame.payload_length);
    assert(mock_led_mode == BOOT_LED_MODE_UPDATE_WINDOW && boot_update_service_take_progress());
}

/* Completion survives traffic even if main never observes TX_DONE. */
static void assert_jump_after_guard(void) {
    const uint32_t guard = BOOT_APPLICATION_HANDOVER_GUARD_MS;
    mock_i2c_state = SLAVE_RX;
    assert(!boot_update_service_take_jump_request(mock_now_ms + guard - 1U));
    mock_i2c_state = SLAVE_TX;
    assert(boot_update_service_take_jump_request(mock_now_ms + guard));
    assert(!boot_update_service_take_jump_request(mock_now_ms + guard + 1U));
    mock_i2c_state = SLAVE_RX_DONE;
}
static void test_jump_deadline(void) {
    for (unsigned late = 0U; late < 2U; ++late) {
        boot_protocol_frame_t frame = {0};
        frame.payload_length = BOOT_PROTOCOL_MIN_PAYLOAD_SIZE;
        frame.payload[0] = PACKET_CMD_ERASE_FLASH;
        frame.payload[1] = PACKET_CMD_TYPE_DATA;
        frame.payload[3] = 0x80U;
        mock_tx_pending = false;
        mock_tx_reserved = false;
        mock_i2c_state = SLAVE_RX_DONE;
        mock_erase_count = 0U;
        mock_flash_result = BSP_FLASH_OK;
        mock_app_valid = true;
        boot_update_service_init();
        boot_update_service_handle_frame(&frame);
        assert_response(0U, PACKET_CMD_ERASE_FLASH, PACKET_ACK_OK, frame.payload_length);
        mock_complete_reads = UINT32_MAX;
        mock_now_ms = UINT32_MAX - 50U;
        const uint32_t start = mock_now_ms;
        frame.payload[0] = PACKET_CMD_JUMP_TO_APP;
        boot_update_service_handle_frame(&frame);
        assert(!boot_update_service_take_jump_request(start + 10000U));
        if (!late)
            assert_response(0U, PACKET_CMD_JUMP_TO_APP, PACKET_ACK_OK, frame.payload_length);
        mock_now_ms = start + 90U;
        mock_i2c_state = SLAVE_RX_DONE;
        boot_update_service_handle_frame(&frame); /* Duplicate must not restart the deadline. */
        frame.payload[0] = PACKET_CMD_ERASE_FLASH;
        boot_update_service_handle_frame(&frame); /* Nor may ERASE cancel the handover. */
        boot_update_service_stats_t stats;
        boot_update_service_get_stats(&stats);
        assert(stats.responses_published == 2U);
        assert(stats.erase_commands == 1U);
        assert(stats.response_busy_drop == 2U);
        if (late) {
            assert(!boot_update_service_take_jump_request(start + 10000U));
            assert_response(0U, PACKET_CMD_JUMP_TO_APP, PACKET_ACK_OK, frame.payload_length);
        } else {
            assert(!boot_update_service_take_jump_request(start + 99U));
        }
        mock_i2c_state = SLAVE_TX;
        assert(boot_update_service_take_jump_request(start + (late ? 10000U : 100U)));
        assert(!boot_update_service_take_jump_request(start + 10001U));
    }
    mock_i2c_state = SLAVE_RX_DONE;
}
#endif

#if BOOT_TEST_UPDATE_MODE == 1
/** @brief Verifies enabled update commands preserve legacy acknowledgements and TX ownership. */
static void test_update_command_responses(void) {
    boot_protocol_frame_t frame = {0};
    boot_update_service_stats_t stats;

    frame.frame_number = 0x35U;
    frame.payload_length = BOOT_PROTOCOL_MIN_PAYLOAD_SIZE;
    frame.payload[1] = PACKET_CMD_TYPE_DATA;
    frame.payload[2] = 0x00U;
    frame.payload[3] = 0x80U;

    mock_erase_count = 0U;
    mock_flash_result = BSP_FLASH_OK;
    mock_app_valid = true;
    mock_tx_pending = false;
    mock_tx_reserved = false;
    mock_i2c_state = SLAVE_RX_DONE;
    boot_update_service_init();
    frame.payload[0] = PACKET_CMD_ERASE_FLASH;
    boot_update_service_handle_frame(&frame);
    assert_response(frame.frame_number, PACKET_CMD_ERASE_FLASH, PACKET_ACK_OK, BOOT_PROTOCOL_MIN_PAYLOAD_SIZE);
    assert(mock_erase_count == bsp_flash_sector_count(APP_FLASH_MAX_SIZE));

    boot_update_service_handle_frame(&frame);
    assert_response(frame.frame_number, PACKET_CMD_ERASE_FLASH, PACKET_ACK_OK, BOOT_PROTOCOL_MIN_PAYLOAD_SIZE);
    assert(mock_erase_count == bsp_flash_sector_count(APP_FLASH_MAX_SIZE));

    frame.payload[0] = PACKET_CMD_APP_DOWNLOAD;
    frame.payload_length = BOOT_PROTOCOL_MIN_PAYLOAD_SIZE + 4U;
    frame.payload[BOOT_PROTOCOL_MIN_PAYLOAD_SIZE] = 0x11U;
    frame.payload[BOOT_PROTOCOL_MIN_PAYLOAD_SIZE + 1U] = 0x22U;
    frame.payload[BOOT_PROTOCOL_MIN_PAYLOAD_SIZE + 2U] = 0x33U;
    frame.payload[BOOT_PROTOCOL_MIN_PAYLOAD_SIZE + 3U] = 0x44U;
    boot_update_service_handle_frame(&frame);
    assert_response(frame.frame_number, PACKET_CMD_APP_DOWNLOAD, PACKET_ACK_OK, BOOT_PROTOCOL_MIN_PAYLOAD_SIZE);
    assert(mock_write_address == APP_FLASH_BASE);
    assert(mock_write_length == 4U);
    assert(memcmp(mock_write_data, &frame.payload[BOOT_PROTOCOL_MIN_PAYLOAD_SIZE], 4U) == 0);

    mock_now_ms = UINT32_MAX - BOOT_APPLICATION_HANDOVER_GUARD_MS / 2U;
    frame.payload[0] = PACKET_CMD_JUMP_TO_APP;
    frame.payload_length = BOOT_PROTOCOL_MIN_PAYLOAD_SIZE;
    boot_update_service_handle_frame(&frame);
    assert(!boot_update_service_take_jump_request(0U));
    assert(!boot_update_service_take_jump_request(mock_now_ms + BOOT_APPLICATION_HANDOVER_GUARD_MS - 1U));
    assert_response(frame.frame_number, PACKET_CMD_JUMP_TO_APP, PACKET_ACK_OK, BOOT_PROTOCOL_MIN_PAYLOAD_SIZE);
    assert_jump_after_guard();

    frame.payload[0] = PACKET_CMD_HANDSHAKE;
    boot_update_service_handle_frame(&frame);
    assert(mock_tx_pending);
    uint8_t unread_response[BOOT_PROTOCOL_MAX_FRAME_SIZE];
    const uint16_t unread_length = mock_tx_length;
    memcpy(unread_response, mock_tx, unread_length);
    const uint32_t erase_count_before_busy = mock_erase_count;
    frame.payload[0] = PACKET_CMD_ERASE_FLASH;
    boot_update_service_handle_frame(&frame);
    assert(mock_erase_count == erase_count_before_busy);
    assert(mock_tx_length == unread_length);
    assert(memcmp(mock_tx, unread_response, unread_length) == 0);
    assert_response(frame.frame_number, PACKET_CMD_HANDSHAKE, PACKET_ACK_OK, BOOT_PROTOCOL_MIN_PAYLOAD_SIZE);

    frame.payload[0] = PACKET_CMD_ERASE_FLASH;
    frame.payload[2] = 0U;
    frame.payload[3] = 0U;
    boot_update_service_handle_frame(&frame);
    assert_response(frame.frame_number, PACKET_CMD_ERASE_FLASH, PACKET_ACK_ADDR_ERROR, BOOT_PROTOCOL_MIN_PAYLOAD_SIZE);

    mock_tx_length = 0U;
    frame.payload[0] = PACKET_CMD_APP_UPLOAD;
    boot_update_service_handle_frame(&frame);
    assert(mock_tx_length == 0U);
    boot_update_service_get_stats(&stats);
    assert(stats.validated_frames == 8U);
    assert(stats.responses_published == 6U);
    assert(stats.unsupported_commands == 1U);
    assert(stats.response_busy_drop == 1U);
    assert(stats.erase_commands == 1U);
    assert(stats.programmed_bytes == 4U);
    assert(stats.flash_errors == 0U);
}

static void test_update_flash_failures(void) {
    boot_protocol_frame_t frame = {0};

    frame.frame_number = 1U;
    frame.payload_length = BOOT_PROTOCOL_MIN_PAYLOAD_SIZE;
    frame.payload[0] = PACKET_CMD_ERASE_FLASH;
    frame.payload[1] = PACKET_CMD_TYPE_DATA;
    frame.payload[3] = 0x80U;
    mock_erase_count = 0U;
    mock_flash_result = -1;
    boot_update_service_init();
    boot_update_service_handle_frame(&frame);
    assert_response(frame.frame_number, PACKET_CMD_ERASE_FLASH, PACKET_ACK_ERROR, BOOT_PROTOCOL_MIN_PAYLOAD_SIZE);
    assert(mock_led_mode == BOOT_LED_MODE_FATAL && !boot_update_service_take_progress());

    mock_flash_result = BSP_FLASH_OK;
    mock_erase_count = 0U;
    boot_update_service_handle_frame(&frame);
    assert_response(frame.frame_number, PACKET_CMD_ERASE_FLASH, PACKET_ACK_OK, BOOT_PROTOCOL_MIN_PAYLOAD_SIZE);
    assert(mock_led_mode == BOOT_LED_MODE_UPDATE_WINDOW && boot_update_service_take_progress());
    frame.payload[0] = PACKET_CMD_JUMP_TO_APP;
    mock_app_valid = false;
    boot_update_service_handle_frame(&frame);
    assert_response(frame.frame_number, PACKET_CMD_JUMP_TO_APP, PACKET_ACK_ERROR, BOOT_PROTOCOL_MIN_PAYLOAD_SIZE);
    assert(!boot_update_service_take_jump_request(0U));
}
#elif BOOT_TEST_UPDATE_MODE == 2
static void test_update_command_responses(void) {
    boot_protocol_frame_t frame = {0};
    boot_update_service_stats_t stats;
    frame.frame_number = 0x35U;
    frame.payload_length = BOOT_PROTOCOL_MIN_PAYLOAD_SIZE;
    frame.payload[1] = PACKET_CMD_TYPE_DATA;
    frame.payload[3] = 0x80U;
    mock_erase_count = 0U;
    mock_write_length = 0U;
    mock_app_valid = true;
    mock_tx_pending = false;
    mock_tx_reserved = false;
    mock_i2c_state = SLAVE_RX_DONE;
    boot_update_service_init();

    frame.payload[0] = PACKET_CMD_ERASE_FLASH;
    boot_update_service_handle_frame(&frame);
    assert_response(frame.frame_number, PACKET_CMD_ERASE_FLASH, PACKET_ACK_OK, BOOT_PROTOCOL_MIN_PAYLOAD_SIZE);
    assert(mock_erase_count == 0U);

    frame.payload[0] = PACKET_CMD_APP_DOWNLOAD;
    frame.payload_length = BOOT_PROTOCOL_MIN_PAYLOAD_SIZE + 4U;
    boot_update_service_handle_frame(&frame);
    assert_response(frame.frame_number, PACKET_CMD_APP_DOWNLOAD, PACKET_ACK_OK, BOOT_PROTOCOL_MIN_PAYLOAD_SIZE);
    assert(mock_write_length == 0U);

    mock_now_ms = UINT32_MAX - BOOT_APPLICATION_HANDOVER_GUARD_MS / 2U;
    frame.payload[0] = PACKET_CMD_JUMP_TO_APP;
    frame.payload_length = BOOT_PROTOCOL_MIN_PAYLOAD_SIZE;
    boot_update_service_handle_frame(&frame);
    assert(!boot_update_service_take_jump_request(0U));
    assert(!boot_update_service_take_jump_request(mock_now_ms + BOOT_APPLICATION_HANDOVER_GUARD_MS - 1U));
    assert_response(frame.frame_number, PACKET_CMD_JUMP_TO_APP, PACKET_ACK_OK, BOOT_PROTOCOL_MIN_PAYLOAD_SIZE);
    assert_jump_after_guard();

    boot_update_service_get_stats(&stats);
    assert(stats.erase_commands == 1U);
    assert(stats.programmed_bytes == 4U);
    assert(stats.flash_errors == 0U);
}

static void test_update_flash_failures(void) {}
#else
static void test_update_command_responses(void) {
    boot_protocol_frame_t frame = {0};
    boot_update_service_stats_t stats;
    frame.frame_number = 0x35U;
    frame.payload_length = BOOT_PROTOCOL_MIN_PAYLOAD_SIZE;
    frame.payload[1] = PACKET_CMD_TYPE_DATA;
    frame.payload[3] = 0x80U;
    mock_erase_count = 0U;
    mock_write_length = 0U;
    mock_app_valid = true;
    mock_tx_length = 0U;
    mock_tx_pending = false;
    mock_tx_reserved = false;
    mock_i2c_state = SLAVE_RX_DONE;
    boot_update_service_init();

    frame.payload[0] = PACKET_CMD_ERASE_FLASH;
    boot_update_service_handle_frame(&frame);
    frame.payload[0] = PACKET_CMD_APP_DOWNLOAD;
    boot_update_service_handle_frame(&frame);
    frame.payload[0] = PACKET_CMD_JUMP_TO_APP;
    boot_update_service_handle_frame(&frame);
    assert(mock_tx_length == 0U);
    assert(!mock_tx_pending);
    assert(mock_erase_count == 0U);
    assert(mock_write_length == 0U);
    assert(!boot_update_service_take_jump_request(0U));
    boot_update_service_get_stats(&stats);
    assert(stats.validated_frames == 3U);
    assert(stats.responses_published == 0U);
    assert(stats.unsupported_commands == 3U);
}

static void test_update_flash_failures(void) {}
#endif

int main(void) {
    boot_protocol_crc_tests_run();
    test_sync_and_callback();
    test_rejection_and_recovery();
    test_maximum_and_timeout();
    test_push_bytes();
    test_chunk_and_legacy_callback();
    test_first_update_request();
    test_update_command_responses();
    test_update_flash_failures();
#if BOOT_TEST_UPDATE_MODE != 0
    test_update_led_and_progress();
    test_jump_deadline();
#endif
    puts("boot_protocol_tests: PASS");
    return 0;
}
