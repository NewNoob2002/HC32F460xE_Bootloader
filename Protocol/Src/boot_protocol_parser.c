#include "boot_protocol_parser.h"

#include <stddef.h>
#include <string.h>
#include "boot_protocol_crc.h"
#include "bsp_critical.h"
#include "bsp_i2c_slave.h"

uint8_t local_buffer[1024];
/** @brief Clears frame collection state without changing callback or statistics. */
static void parser_reset_frame(boot_protocol_parser_t* parser) {
    parser->state = BOOT_PARSER_SYNC_1;
    parser->payload_index = 0U;
    parser->received_crc = 0U;
    parser->header_index = 0U;
    parser->crc_index = 0U;
    parser->frame.payload_length = 0U;
}

/** @brief Processes one byte while searching for the three-byte synchronization word. */
static void parser_push_sync(boot_protocol_parser_t* parser, uint8_t byte) {
    switch (parser->state) {
        case BOOT_PARSER_SYNC_1:
            if (byte == BOOT_PROTOCOL_SYNC_1)
                parser->state = BOOT_PARSER_SYNC_2;
            else
                ++parser->stats.sync_error;
            break;
        case BOOT_PARSER_SYNC_2:
            if (byte == BOOT_PROTOCOL_SYNC_2)
                parser->state = BOOT_PARSER_SYNC_3;
            else {
                ++parser->stats.sync_error;
                parser->state = (byte == BOOT_PROTOCOL_SYNC_1) ? BOOT_PARSER_SYNC_2 : BOOT_PARSER_SYNC_1;
            }
            break;
        case BOOT_PARSER_SYNC_3:
            if (byte == BOOT_PROTOCOL_SYNC_3) {
                parser->state = BOOT_PARSER_HEADER;
                parser->header_index = 0U;
            } else {
                ++parser->stats.sync_error;
                parser->state = (byte == BOOT_PROTOCOL_SYNC_1) ? BOOT_PARSER_SYNC_2 : BOOT_PARSER_SYNC_1;
            }
            break;
        default:
            ++parser->stats.overflow_error;
            parser_reset_frame(parser);
            break;
    }
}

/** @brief Validates a completed four-byte frame header and starts payload collection. */
static void parser_finish_header(boot_protocol_parser_t* parser) {
    const uint8_t frame_number = parser->header[0];
    const uint16_t payload_length = (uint16_t)((uint16_t)parser->header[2] | ((uint16_t)parser->header[3] << 8U));

    if (parser->header[1] != (uint8_t)(frame_number ^ 0xFFU)) {
        ++parser->stats.frame_drop;
        parser_reset_frame(parser);
        return;
    }
    if ((payload_length < BOOT_PROTOCOL_MIN_PAYLOAD_SIZE) || (payload_length > BOOT_PROTOCOL_MAX_PAYLOAD_SIZE)) {
        ++parser->stats.length_error;
        ++parser->stats.frame_drop;
        parser_reset_frame(parser);
        return;
    }
    parser->frame.frame_number = frame_number;
    parser->frame.payload_length = payload_length;
    parser->payload_index = 0U;
    parser->state = BOOT_PARSER_PAYLOAD;
}

void BootProtocolParserInit(boot_protocol_parser_t* parser) {
    if (parser == NULL)
        return;
    memset(parser, 0, sizeof(*parser));
    parser_reset_frame(parser);
}

void BootProtocolParserRegisterCallback(boot_protocol_parser_t* parser, boot_protocol_frame_callback_t callback,
                                        void* context) {
    if (parser == NULL)
        return;
    parser->callback = callback;
    parser->callback_context = context;
}

bool BootProtocolParserPushByte(boot_protocol_parser_t* parser, uint8_t byte) {
    if (parser == NULL)
        return false;
    ++parser->stats.bytes_received;

    if (parser->state <= BOOT_PARSER_SYNC_3) {
        parser_push_sync(parser, byte);
        return false;
    }
    switch (parser->state) {
        case BOOT_PARSER_HEADER:
            parser->header[parser->header_index++] = byte;
            if (parser->header_index == sizeof(parser->header))
                parser_finish_header(parser);
            break;
        case BOOT_PARSER_PAYLOAD:
            if (parser->payload_index >= parser->frame.payload_length) {
                ++parser->stats.overflow_error;
                ++parser->stats.frame_drop;
                parser_reset_frame(parser);
                break;
            }
            parser->frame.payload[parser->payload_index++] = byte;
            if (parser->payload_index == parser->frame.payload_length) {
                parser->received_crc = 0U;
                parser->crc_index = 0U;
                parser->state = BOOT_PARSER_CRC;
            }
            break;
        case BOOT_PARSER_CRC:
            if (parser->crc_index == 0U) {
                parser->received_crc = byte;
                ++parser->crc_index;
            } else {
                const uint16_t calculated_crc =
                    BootProtocolCrcCalculate(parser->frame.payload, parser->frame.payload_length);
                parser->received_crc |= (uint16_t)byte << 8U;
                if (parser->received_crc == calculated_crc) {
                    ++parser->stats.frame_received;
                    if (parser->callback != NULL)
                        parser->callback(&parser->frame, parser->callback_context);
                    else
                        ++parser->stats.frame_drop;
                    parser_reset_frame(parser);
                    return true;
                }
                ++parser->stats.crc_error;
                ++parser->stats.frame_drop;
                parser_reset_frame(parser);
            }
            break;
        default:
            ++parser->stats.overflow_error;
            ++parser->stats.frame_drop;
            parser_reset_frame(parser);
            break;
    }
    return false;
}

void BootProtocolParserProcess(boot_protocol_parser_t* parser) {
    if (parser == NULL)
        return;

    size_t copied_length = 0U;
    if (bsp_i2c_slave_get_state() == SLAVE_RX_DONE) {
        bsp_irq_state_t irq_state = bsp_enter_critical();
        while ((rxBufferAvailable() > 0) && (copied_length < sizeof(local_buffer))) {
            local_buffer[copied_length] = rxBufferRead();
            ++copied_length;
        }
        bsp_exit_critical(irq_state);
    }

    for (size_t i = 0U; i < copied_length; ++i) {
        BootProtocolParserPushByte(parser, local_buffer[i]);
    }
    bsp_i2c_slave_update();
}

void BootProtocolParserTimeout(boot_protocol_parser_t* parser) {
    if ((parser == NULL) || (parser->state == BOOT_PARSER_SYNC_1))
        return;
    ++parser->stats.frame_drop;
    parser_reset_frame(parser);
}

void BootProtocolParserGetStats(const boot_protocol_parser_t* parser, boot_protocol_parser_stats_t* stats) {
    if ((parser != NULL) && (stats != NULL))
        *stats = parser->stats;
}

bool BootProtocolParserHasPartialFrame(const boot_protocol_parser_t* parser) {
    return (parser != NULL) && (parser->state != BOOT_PARSER_SYNC_1);
}
