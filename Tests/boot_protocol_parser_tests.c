#include "boot_protocol_crc.h"
#include "boot_protocol_parser.h"
#include "boot_update_service.h"
#include "bsp_i2c_slave.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t mock_rx[BOOT_PROTOCOL_MAX_FRAME_SIZE * 2U];
static size_t mock_rx_length;
static size_t mock_rx_index;
static uint8_t mock_tx[BSP_I2C_TX_CAPACITY];
static uint16_t mock_tx_length;
static boot_protocol_frame_t callback_frame;
static uint32_t callback_count;

void boot_protocol_crc_tests_run(void);

int rxBufferAvailable(void) {
    return (int)(mock_rx_length - mock_rx_index);
}

uint8_t rxBufferRead(void) {
    return (mock_rx_index < mock_rx_length) ? mock_rx[mock_rx_index++] : 0U;
}

int txBufferAvailable(void) {
    return (int)mock_tx_length;
}

int txBufferWrite(uint8_t* buffer, const uint16_t length) {
    assert(buffer != NULL);
    assert(length <= sizeof(mock_tx));
    memcpy(mock_tx, buffer, length);
    mock_tx_length = length;
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
    const uint16_t crc = BootProtocolCrcCalculate(payload, payload_length);
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
    uint32_t completed = 0U;
    for (size_t index = 0U; index < length; ++index)
        completed += BootProtocolParserPushByte(parser, bytes[index]) ? 1U : 0U;
    return completed;
}

/** @brief Verifies synchronization recovery, callback content, and minimum payload framing. */
static void test_sync_and_callback(void) {
    boot_protocol_parser_t parser;
    uint8_t payload[BOOT_PROTOCOL_MIN_PAYLOAD_SIZE] = {0x20U, 0x11U};
    uint8_t frame[BOOT_PROTOCOL_MAX_FRAME_SIZE];
    uint8_t stream[BOOT_PROTOCOL_MAX_FRAME_SIZE + 9U];
    static const uint8_t noise[] = {0x55U, 0xAAU, 0x44U, 0x00U, 0xAAU, 0xAAU, 0xAAU};
    const size_t length = make_frame(frame, 7U, payload, sizeof(payload));

    BootProtocolParserInit(&parser);
    BootProtocolParserRegisterCallback(&parser, capture_frame, NULL);
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

    BootProtocolParserInit(&parser);
    BootProtocolParserRegisterCallback(&parser, capture_frame, NULL);
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
    BootProtocolParserGetStats(&parser, &stats);
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
    BootProtocolParserInit(&parser);
    BootProtocolParserRegisterCallback(&parser, capture_frame, NULL);
    assert(feed_bytes(&parser, frame, 8U) == 0U);
    assert(BootProtocolParserHasPartialFrame(&parser));
    BootProtocolParserTimeout(&parser);
    assert(!BootProtocolParserHasPartialFrame(&parser));
    assert(feed_bytes(&parser, frame, length) == 1U);
    assert(feed_bytes(&parser, frame, length) == 1U);
}

/** @brief Verifies the parser callback produces the legacy handshake response through txBufferWrite. */
static void test_process_and_legacy_callback(void) {
    static const uint8_t request[] = {0xAAU, 0x44U, 0x18U, 0x01U, 0xFEU, 0x0CU, 0x00U, 0x20U, 0x11U, 0U,   0U,
                                      0U,    0U,    0U,    0U,    0U,    0U,    0U,    0U,    0xD4U, 0xE3U};
    static const uint8_t response[] = {0xAAU, 0x44U, 0x18U, 0x01U, 0xFEU, 0x0CU, 0x00U, 0x20U, 0x00U, 0U,   0U,
                                       0U,    0U,    0U,    0U,    0U,    0U,    0U,    0U,    0xA0U, 0x6EU};
    boot_protocol_parser_t parser;
    boot_update_service_stats_t stats;

    memcpy(mock_rx, request, sizeof(request));
    mock_rx_length = sizeof(request);
    mock_rx_index = 0U;
    mock_tx_length = 0U;
    BootUpdateServiceInit();
    BootProtocolParserInit(&parser);
    BootProtocolParserRegisterCallback(&parser, BootUpdateServiceFrameCallback, NULL);
    BootProtocolParserProcess(&parser);
    assert(mock_rx_index == sizeof(request));
    assert(mock_tx_length == sizeof(response));
    assert(memcmp(mock_tx, response, sizeof(response)) == 0);
    BootUpdateServiceGetStats(&stats);
    assert(stats.validated_frames == 1U);
    assert(stats.responses_published == 1U);
}

int main(void) {
    boot_protocol_crc_tests_run();
    test_sync_and_callback();
    test_rejection_and_recovery();
    test_maximum_and_timeout();
    test_process_and_legacy_callback();
    puts("boot_protocol_tests: PASS");
    return 0;
}
