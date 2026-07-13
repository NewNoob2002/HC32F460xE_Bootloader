#include "boot_app.h"
#include "bsp_i2c_slave.h"
#include "legacy_codec.h"
#include "legacy_crc16.h"
#include "legacy_parser.h"
#include "legacy_simulation.h"
#include "legacy_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint8_t request[] = {
    0xAA,0x44,0x18,0x01,0xFE,0x0C,0x00,0x20,0x11,0,0,0,0,0,0,0,0,0,0,0xD4,0xE3
};
static const uint8_t response[] = {
    0xAA,0x44,0x18,0x01,0xFE,0x0C,0x00,0x20,0x00,0,0,0,0,0,0,0,0,0,0,0xA0,0x6E
};

static uint8_t mock_rx[BSP_I2C_RX_CAPACITY];
static size_t mock_rx_length;
static bool mock_rx_ready;
static uint8_t mock_tx[BSP_I2C_TX_CAPACITY];
static size_t mock_tx_length;
static bool mock_tx_ready;
static bool mock_tx_consumed;
static bsp_i2c_slave_counters_t mock_transport_counters;

bool bsp_i2c_slave_take_rx_transaction(uint8_t* destination, size_t capacity, size_t* length) {
    if (!mock_rx_ready || (destination == NULL) || (length == NULL) ||
        (capacity < mock_rx_length)) {
        return false;
    }
    memcpy(destination, mock_rx, mock_rx_length);
    *length = mock_rx_length;
    mock_rx_ready = false;
    return true;
}

bool bsp_i2c_slave_publish_response(const uint8_t* source, size_t length) {
    if ((source == NULL) || (length == 0U) || (length > sizeof(mock_tx)) || mock_tx_ready) {
        return false;
    }
    memcpy(mock_tx, source, length);
    mock_tx_length = length;
    mock_tx_ready = true;
    mock_tx_consumed = false;
    return true;
}

bool bsp_i2c_slave_response_consumed(void) {
    const bool consumed = mock_tx_consumed;
    mock_tx_consumed = false;
    return consumed;
}

void bsp_i2c_slave_get_counters(bsp_i2c_slave_counters_t* counters) {
    if (counters != NULL) {
        *counters = mock_transport_counters;
    }
}

static void mock_transport_reset(void) {
    memset(mock_rx, 0, sizeof(mock_rx));
    memset(mock_tx, 0, sizeof(mock_tx));
    memset(&mock_transport_counters, 0, sizeof(mock_transport_counters));
    mock_rx_length = 0U;
    mock_rx_ready = false;
    mock_tx_length = 0U;
    mock_tx_ready = false;
    mock_tx_consumed = false;
}

static void mock_queue_rx(const uint8_t* source, size_t length) {
    assert(source != NULL);
    assert(length <= sizeof(mock_rx));
    memcpy(mock_rx, source, length);
    mock_rx_length = length;
    mock_rx_ready = true;
}

static void mock_consume_response(void) {
    assert(mock_tx_ready);
    mock_tx_ready = false;
    mock_tx_consumed = true;
    ++mock_transport_counters.tx_complete_reads;
}

static uint16_t make_frame(uint8_t* destination, uint8_t frame_number, uint8_t command,
                           uint8_t type, uint32_t address, const uint8_t* data,
                           size_t data_length, size_t instruction_extension) {
    assert(destination != NULL);
    assert(data_length <= LEGACY_MAX_DATA_SIZE);
    assert(instruction_extension <= (LEGACY_MAX_PAYLOAD_SIZE - LEGACY_INSTRUCTION_SIZE - data_length));
    const size_t payload_length = LEGACY_INSTRUCTION_SIZE + instruction_extension + data_length;
    const size_t total = LEGACY_FRAME_HEADER_SIZE + payload_length + LEGACY_FRAME_CRC_SIZE;
    memset(destination, 0, total);
    destination[0] = 0xAAU;
    destination[1] = 0x44U;
    destination[2] = 0x18U;
    destination[3] = frame_number;
    destination[4] = (uint8_t)(frame_number ^ 0xFFU);
    destination[5] = (uint8_t)payload_length;
    destination[6] = (uint8_t)(payload_length >> 8U);
    destination[7] = command;
    destination[8] = type;
    destination[9] = (uint8_t)address;
    destination[10] = (uint8_t)(address >> 8U);
    destination[11] = (uint8_t)(address >> 16U);
    destination[12] = (uint8_t)(address >> 24U);
    if (data_length != 0U)
        memcpy(&destination[LEGACY_FRAME_HEADER_SIZE + LEGACY_INSTRUCTION_SIZE + instruction_extension],
               data, data_length);
    const uint16_t crc = legacy_crc16_xmodem(&destination[LEGACY_FRAME_HEADER_SIZE], payload_length);
    destination[LEGACY_FRAME_HEADER_SIZE + payload_length] = (uint8_t)crc;
    destination[LEGACY_FRAME_HEADER_SIZE + payload_length + 1U] = (uint8_t)(crc >> 8U);
    assert(total <= UINT16_MAX);
    return (uint16_t)total;
}

static void assert_response_crc(const uint8_t* encoded, size_t length, uint8_t ack) {
    assert(encoded != NULL);
    const uint16_t payload = (uint16_t)encoded[5] | ((uint16_t)encoded[6] << 8U);
    assert(length == LEGACY_FRAME_HEADER_SIZE + payload + LEGACY_FRAME_CRC_SIZE);
    assert(encoded[8] == ack);
    const uint16_t wire_crc = (uint16_t)encoded[LEGACY_FRAME_HEADER_SIZE + payload] |
                              ((uint16_t)encoded[LEGACY_FRAME_HEADER_SIZE + payload + 1U] << 8U);
    assert(wire_crc == legacy_crc16_xmodem(&encoded[LEGACY_FRAME_HEADER_SIZE], payload));
}

static void test_crc_and_handshake(void) {
    assert(legacy_crc16_xmodem((const uint8_t*)"123456789", 9U) == 0x31C3U);
    legacy_parser_t parser;
    legacy_frame_t frame;
    legacy_parser_init(&parser);
    for (size_t i = 0U; i < sizeof(request); ++i)
        assert(legacy_parser_feed(&parser, &request[i], 1U, (uint32_t)i) ==
               (i + 1U == sizeof(request) ? LEGACY_PARSE_FRAME : LEGACY_PARSE_MORE));
    assert(legacy_parser_take_frame(&parser, &frame));
    uint8_t encoded[LEGACY_MAX_FRAME_SIZE];
    size_t length = 0U;
    assert(legacy_codec_handshake_response(&frame, encoded, sizeof(encoded), &length));
    assert(length == sizeof(response));
    assert(memcmp(encoded, response, sizeof(response)) == 0);
    assert(!legacy_codec_handshake_response(&frame, encoded, sizeof(response) - 1U, &length));
}

static void test_parser_rejection_and_timeout(void) {
    legacy_parser_t parser;
    legacy_parser_init(&parser);
    uint8_t bad[sizeof(request)];
    memcpy(bad, request, sizeof(bad));
    bad[4] = 0U;
    assert(legacy_parser_feed(&parser, bad, 7U, 0U) == LEGACY_PARSE_INVALID);
    legacy_parser_init(&parser);
    memcpy(bad, request, sizeof(bad));
    bad[5] = 11U;
    assert(legacy_parser_feed(&parser, bad, 7U, 0U) == LEGACY_PARSE_INVALID);
    legacy_parser_init(&parser);
    uint8_t garbage[] = { 0x00U, 0xAAU, 0xAAU, 0x44U, 0x18U };
    assert(legacy_parser_feed(&parser, garbage, sizeof(garbage), 1U) == LEGACY_PARSE_MORE);
    assert(parser.length == 3U);
    legacy_parser_on_timeout(&parser);
    assert(parser.length == 0U);

    uint8_t frame[LEGACY_MAX_FRAME_SIZE + 1U];
    uint8_t data[LEGACY_MAX_DATA_SIZE] = {0};
    size_t length = make_frame(frame, 2U, LEGACY_CMD_APP_DOWNLOAD, LEGACY_CMD_TYPE_DATA,
                               0x00008000UL, data, sizeof(data), 0U);
    legacy_parser_init(&parser);
    assert(length == LEGACY_MAX_FRAME_SIZE);
    assert(legacy_parser_feed(&parser, frame, length, 2U) == LEGACY_PARSE_FRAME);

    legacy_parser_init(&parser);
    memcpy(frame, request, LEGACY_FRAME_HEADER_SIZE);
    frame[3] = 3U;
    frame[4] = (uint8_t)(3U ^ 0xFFU);
    frame[5] = (uint8_t)525U;
    frame[6] = (uint8_t)(525U >> 8U);
    assert(legacy_parser_feed(&parser, frame, LEGACY_FRAME_HEADER_SIZE, 3U) == LEGACY_PARSE_INVALID);

    legacy_parser_init(&parser);
    memcpy(bad, request, sizeof(bad));
    bad[0] = 0xABU;
    assert(legacy_parser_feed(&parser, bad, sizeof(bad), 4U) == LEGACY_PARSE_MORE);

    legacy_parser_init(&parser);
    memcpy(bad, request, sizeof(bad));
    bad[sizeof(bad) - 1U] ^= 1U;
    assert(legacy_parser_feed(&parser, bad, sizeof(bad), 5U) == LEGACY_PARSE_INVALID);

    legacy_parser_init(&parser);
    assert(legacy_parser_feed(&parser, request, sizeof(request) - 1U, 6U) == LEGACY_PARSE_MORE);
    legacy_parser_on_timeout(&parser);
    assert(parser.length == 0U);
}

static void test_simulated_commands(void) {
    uint8_t frame_bytes[LEGACY_MAX_FRAME_SIZE];
    uint8_t encoded[LEGACY_MAX_FRAME_SIZE];
    uint8_t data[LEGACY_MAX_DATA_SIZE];
    legacy_frame_t frame;
    legacy_simulation_result_t result;
    size_t encoded_length = 0U;
    for (size_t index = 0U; index < sizeof(data); ++index)
        data[index] = (uint8_t)index;

    legacy_simulation_init();
    memcpy(frame.bytes, request, sizeof(request));
    frame.length = sizeof(request);
    assert(legacy_simulation_process(&frame, encoded, sizeof(encoded), &encoded_length, &result));
    assert(encoded_length == sizeof(response));
    assert(memcmp(encoded, response, sizeof(response)) == 0);
    assert(g_legacy_simulation_stats.handshake_count == 1U);

    frame.length = make_frame(frame.bytes, 2U, LEGACY_CMD_ERASE_FLASH, LEGACY_CMD_TYPE_DATA,
                              0x00040000UL, NULL, 0U, 0U);
    assert(legacy_simulation_process(&frame, encoded, sizeof(encoded), &encoded_length, &result));
    assert(encoded_length == 21U);
    assert(result.address == 0x00040000UL);
    assert_response_crc(encoded, encoded_length, LEGACY_ACK_OK);
    assert(g_legacy_simulation_stats.erase_seen);

    frame.length = make_frame(frame.bytes, 3U, LEGACY_CMD_ERASE_FLASH, 0x11U,
                              0x12345678UL, NULL, 0U, 0U);
    assert(legacy_simulation_process(&frame, encoded, sizeof(encoded), &encoded_length, &result));
    assert((result.warnings & LEGACY_SIM_WARNING_ERASE_TYPE) != 0U);
    assert((result.warnings & LEGACY_SIM_WARNING_MULTIPLE_ERASE) != 0U);
    assert_response_crc(encoded, encoded_length, LEGACY_ACK_OK);

    frame.length = make_frame(frame.bytes, 4U, LEGACY_CMD_APP_DOWNLOAD, LEGACY_CMD_TYPE_DATA,
                              0x00008000UL, data, 1U, 0U);
    assert(legacy_simulation_process(&frame, encoded, sizeof(encoded), &encoded_length, &result));
    assert(result.data_length == 1U);
    assert(result.data_signature == legacy_crc16_xmodem(data, 1U));
    assert_response_crc(encoded, encoded_length, LEGACY_ACK_OK);

    frame.length = make_frame(frame.bytes, 5U, LEGACY_CMD_APP_DOWNLOAD, LEGACY_CMD_TYPE_DATA,
                              0x00008001UL, data, sizeof(data), 0U);
    assert(legacy_simulation_process(&frame, encoded, sizeof(encoded), &encoded_length, &result));
    assert(result.data_length == LEGACY_MAX_DATA_SIZE);
    assert_response_crc(encoded, encoded_length, LEGACY_ACK_OK);

    frame.length = make_frame(frame.bytes, 6U, LEGACY_CMD_APP_DOWNLOAD, LEGACY_CMD_TYPE_DATA,
                              0x00008201UL, data, 7U, 0U);
    assert(legacy_simulation_process(&frame, encoded, sizeof(encoded), &encoded_length, &result));
    assert(result.data_length == 7U);
    assert(g_legacy_simulation_stats.download_total_bytes == 520U);
    assert(g_legacy_simulation_stats.download_first_address == 0x00008000UL);
    assert(g_legacy_simulation_stats.download_last_address == 0x00008201UL);
    assert(g_legacy_simulation_stats.download_last_end == 0x00008208UL);

    frame.length = make_frame(frame.bytes, 6U, LEGACY_CMD_APP_DOWNLOAD, LEGACY_CMD_TYPE_DATA,
                              0x00007FFFUL, NULL, 0U, 0U);
    assert(legacy_simulation_process(&frame, encoded, sizeof(encoded), &encoded_length, &result));
    assert((result.warnings & LEGACY_SIM_WARNING_ZERO_DATA) != 0U);
    assert((result.warnings & LEGACY_SIM_WARNING_ADDRESS_LOW) != 0U);
    assert((result.warnings & LEGACY_SIM_WARNING_NON_CONTIGUOUS) != 0U);
    assert((result.warnings & LEGACY_SIM_WARNING_FRAME_REPEATED) != 0U);
    assert_response_crc(encoded, encoded_length, LEGACY_ACK_OK);

    frame.length = make_frame(frame.bytes, 9U, LEGACY_CMD_APP_DOWNLOAD, LEGACY_CMD_TYPE_DATA,
                              0x00079FFFUL, data, 2U, 0U);
    assert(legacy_simulation_process(&frame, encoded, sizeof(encoded), &encoded_length, &result));
    assert((result.warnings & LEGACY_SIM_WARNING_ADDRESS_HIGH) != 0U);
    assert((result.warnings & LEGACY_SIM_WARNING_FRAME_SKIPPED) != 0U);
    assert_response_crc(encoded, encoded_length, LEGACY_ACK_OK);

    frame.length = make_frame(frame.bytes, 10U, LEGACY_CMD_JUMP_TO_APP, 0x11U,
                              0U, NULL, 0U, 4U);
    assert(legacy_simulation_process(&frame, encoded, sizeof(encoded), &encoded_length, &result));
    assert(encoded_length == frame.length);
    assert_response_crc(encoded, encoded_length, LEGACY_ACK_OK);
    legacy_simulation_response_consumed(LEGACY_CMD_JUMP_TO_APP);
    assert(g_legacy_simulation_stats.jump_ack_consumed_count == 1U);

    frame.length = make_frame(frame.bytes, 11U, LEGACY_CMD_APP_UPLOAD, 0x11U,
                              0x00008000UL, NULL, 0U, 0U);
    assert(legacy_simulation_process(&frame, encoded, sizeof(encoded), &encoded_length, &result));
    assert(result.ack == LEGACY_ACK_ERROR);
    assert_response_crc(encoded, encoded_length, LEGACY_ACK_ERROR);

    assert(g_legacy_simulation_stats.valid_frame_count == 10U);
    assert(g_legacy_simulation_stats.erase_count == 2U);
    assert(g_legacy_simulation_stats.download_count == 5U);
    assert(g_legacy_simulation_stats.jump_count == 1U);
    assert(g_legacy_simulation_stats.upload_count == 1U);

    legacy_simulation_init();
    frame.length = make_frame(frame_bytes, 1U, LEGACY_CMD_APP_DOWNLOAD, LEGACY_CMD_TYPE_DATA,
                              0x00009000UL, data, 8U, 0U);
    memcpy(frame.bytes, frame_bytes, frame.length);
    assert(legacy_simulation_process(&frame, encoded, sizeof(encoded), &encoded_length, &result));
    assert((result.warnings & LEGACY_SIM_WARNING_DOWNLOAD_BEFORE_ERASE) != 0U);
    assert_response_crc(encoded, encoded_length, LEGACY_ACK_OK);

    legacy_simulation_init();
    frame.length = make_frame(frame.bytes, 1U, LEGACY_CMD_JUMP_TO_APP, 0x11U,
                              0U, NULL, 0U, 0U);
    assert(legacy_simulation_process(&frame, encoded, sizeof(encoded), &encoded_length, &result));
    assert((result.warnings & LEGACY_SIM_WARNING_JUMP_BEFORE_DOWNLOAD) != 0U);
    assert_response_crc(encoded, encoded_length, LEGACY_ACK_OK);

    legacy_simulation_note_malformed_frame();
    assert(g_legacy_simulation_stats.malformed_frame_count == 1U);
    assert(g_legacy_simulation_stats.valid_frame_count == 1U);
}

static void test_protocol_service_jump_ack_stays_simulated(void) {
    uint8_t frame[LEGACY_MAX_FRAME_SIZE];
    mock_transport_reset();
    legacy_protocol_service_init(0U);

    const size_t length = make_frame(frame, 1U, LEGACY_CMD_JUMP_TO_APP, 0x11U,
                                     0U, NULL, 0U, 0U);
    mock_queue_rx(frame, length);
    legacy_protocol_service_poll(1U);
    assert(mock_tx_ready);
    assert(mock_tx_length == length);
    assert_response_crc(mock_tx, mock_tx_length, LEGACY_ACK_OK);
    assert(g_legacy_simulation_stats.jump_count == 1U);
    assert(g_legacy_simulation_stats.jump_ack_consumed_count == 0U);

    mock_consume_response();
    legacy_protocol_service_poll(2U);
    assert(!mock_tx_ready);
    assert(g_legacy_simulation_stats.jump_ack_consumed_count == 1U);

    legacy_protocol_service_init(10U);
    uint8_t malformed[sizeof(request)];
    memcpy(malformed, request, sizeof(malformed));
    malformed[sizeof(malformed) - 1U] ^= 1U;
    mock_queue_rx(malformed, sizeof(malformed));
    legacy_protocol_service_poll(11U);
    assert(!mock_tx_ready);
    assert(g_legacy_simulation_stats.valid_frame_count == 0U);
    assert(g_legacy_simulation_stats.malformed_frame_count == 1U);
}

static void test_boot_modes(void) {
    boot_context_t context = {0};
    boot_app_select_mode(&context, false, false);
    assert(context.mode == BOOT_MODE_RECOVERY);
    boot_app_select_mode(&context, true, true);
    assert(context.mode == BOOT_MODE_UPDATE_WINDOW);
    boot_app_select_mode(&context, true, false);
    assert(context.mode == BOOT_MODE_START_APPLICATION);
}

int main(void) {
    test_crc_and_handshake();
    test_parser_rejection_and_timeout();
    test_simulated_commands();
    test_protocol_service_jump_ack_stays_simulated();
    test_boot_modes();
    puts("boot_host_tests: PASS");
    return 0;
}
