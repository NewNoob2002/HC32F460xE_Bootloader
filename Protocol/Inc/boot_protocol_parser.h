#ifndef BOOT_PROTOCOL_PARSER_H
#define BOOT_PROTOCOL_PARSER_H

#include "boot_protocol_types.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BOOT_PARSER_SYNC_1,
    BOOT_PARSER_SYNC_2,
    BOOT_PARSER_SYNC_3,
    BOOT_PARSER_HEADER,
    BOOT_PARSER_PAYLOAD,
    BOOT_PARSER_CRC
} boot_parser_state_t;

typedef struct {
    uint32_t bytes_received;
    uint32_t frame_received;
    uint32_t crc_error;
    uint32_t sync_error;
    uint32_t length_error;
    uint32_t overflow_error;
    uint32_t frame_drop;
} boot_protocol_parser_stats_t;

typedef void (*boot_protocol_frame_callback_t)(const boot_protocol_frame_t* frame, void* context);

typedef struct {
    boot_parser_state_t state;
    boot_protocol_frame_t frame;
    boot_protocol_parser_stats_t stats;
    boot_protocol_frame_callback_t callback;
    void* callback_context;
    uint16_t payload_index;
    uint16_t received_crc;
    uint8_t header[4];
    uint8_t header_index;
    uint8_t crc_index;
} boot_protocol_parser_t;

/** @brief Initializes a parser and clears its callback and statistics. */
void BootProtocolParserInit(boot_protocol_parser_t* parser);

/**
 * @brief Registers the callback invoked after a complete frame passes CRC validation.
 * @param parser Parser instance.
 * @param callback Frame callback, or NULL to discard validated frames.
 * @param context Opaque callback context.
 */
void BootProtocolParserRegisterCallback(boot_protocol_parser_t* parser, boot_protocol_frame_callback_t callback,
                                        void* context);

/**
 * @brief Pushes one byte through the non-blocking parser state machine.
 * @param parser Parser instance.
 * @param byte Next stream byte.
 * @return true only when this byte completes a valid frame.
 */
bool BootProtocolParserPushByte(boot_protocol_parser_t* parser, uint8_t byte);

/** @brief Consumes currently available bytes through the I2C RX buffer API. */
void BootProtocolParserProcess(boot_protocol_parser_t* parser);

/** @brief Discards an incomplete frame after a caller-managed timeout. */
void BootProtocolParserTimeout(boot_protocol_parser_t* parser);

/** @brief Copies parser statistics into caller-owned storage. */
void BootProtocolParserGetStats(const boot_protocol_parser_t* parser, boot_protocol_parser_stats_t* stats);

/** @brief Reports whether synchronization or frame collection is in progress. */
bool BootProtocolParserHasPartialFrame(const boot_protocol_parser_t* parser);

#endif
