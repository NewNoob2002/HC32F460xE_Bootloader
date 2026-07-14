#ifndef BOOT_UPDATE_SERVICE_H
#define BOOT_UPDATE_SERVICE_H

#include "boot_protocol_types.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t validated_frames;
    uint32_t responses_published;
    uint32_t response_busy_drop;
    uint32_t unsupported_commands;
    uint32_t erase_commands;
    uint32_t programmed_bytes;
    uint32_t flash_errors;
} boot_update_service_stats_t;

typedef enum {
    PACKET_CMD_TYPE_CONTROL = 0x11,
    PACKET_CMD_TYPE_DATA = 0x12,
} en_packet_cmd_type_t;

typedef enum {
    PACKET_CMD_HANDSHAKE = 0x20,
    PACKET_CMD_JUMP_TO_APP = 0x21,
    PACKET_CMD_APP_DOWNLOAD = 0x22,
    PACKET_CMD_APP_UPLOAD = 0x23,
    PACKET_CMD_ERASE_FLASH = 0x24,
} en_packet_cmd_t;

typedef enum {
    PACKET_ACK_OK = 0x00,
    PACKET_ACK_ERROR = 0x01,
    PACKET_ACK_ABORT = 0x02,
    PACKET_ACK_TIMEOUT = 0x03,
    PACKET_ACK_ADDR_ERROR = 0x04,
} en_packet_status_t;

/* Frame and packet size */
#define PACKET_INSTRUCT_SEGMENT_SIZE 12
#define PACKET_DATA_SEGMENT_SIZE     512
#define PACKET_MIN_SIZE              PACKET_INSTRUCT_SEGMENT_SIZE
#define PACKET_MAX_SIZE              (PACKET_DATA_SEGMENT_SIZE + PACKET_INSTRUCT_SEGMENT_SIZE)

#define FRAME_SHELL_SIZE             0x09 ///0x07

/* Frame structure defines */
#define FRAME_HEAD_INDEX             0x00
#define FRAME_NUM_INDEX              0x03
#define FRAME_XORNUM_INDEX           0x04
#define FRAME_LENGTH_INDEX           0x05
#define FRAME_PACKET_INDEX           0x07

#define FRAME_RECV_TIMEOUT           5 // ms
#define FRAME_NUM_XOR_BYTE           0xFF

/* Packet structure defines */
#define PACKET_CMD_INDEX             (FRAME_PACKET_INDEX + 0x00)
#define PACKET_TYPE_INDEX            (FRAME_PACKET_INDEX + 0x01)
#define PACKET_RESULT_INDEX          (FRAME_PACKET_INDEX + 0x01)
#define PACKET_ADDRESS_INDEX         (FRAME_PACKET_INDEX + 0x02)
#define PACKET_DATA_INDEX            (FRAME_PACKET_INDEX + PACKET_INSTRUCT_SEGMENT_SIZE)

#define BOOT_MSG_SYN_BYTE1           0xaa
#define BOOT_MSG_SYN_BYTE2           0x44
#define BOOT_MSG_SYN_BYTE3           0x18

#define BOOT_MSG_CRC_LEN             2
#define FRAME_HEADER_LEN             FRAME_PACKET_INDEX
#define BOOT_MSG_MAX_LEN (FRAME_HEADER_LEN + PACKET_INSTRUCT_SEGMENT_SIZE + PACKET_DATA_SEGMENT_SIZE + BOOT_MSG_CRC_LEN)

//    |<------ 8 bytes --------> |<------- PacketData --------->|<- 2 bytes   |                                              |             |
//    +----------+--------+----------------+---------+----------+-------------+
//    | Synchron   Frame_Header |  FRAME_LENGTH_INDEX Bytes     |   CRC-16    |
//    |  24 bits     40bits     |                               |   16 bits   |
//    | 0xAA 44 18              |                               |             |
//    +----------+--------+----------------+---------+----------+-------------+
//                              |                               |
//                              |<---------- CRC -------------->|

/** @brief Initializes protocol command-service state. */
void BootUpdateServiceInit(void);

/**
 * @brief Handles one validated frame and publishes any protocol response through the TX buffer API.
 * @param frame CRC-validated protocol frame.
 */
void BootUpdateServiceHandleFrame(const boot_protocol_frame_t* frame);

/**
 * @brief Parser callback matching the legacy CustomDataProcess handoff pattern.
 * @param frame CRC-validated protocol frame.
 * @param context Unused callback context.
 */
void BootUpdateServiceFrameCallback(const boot_protocol_frame_t* frame, void* context);

/** @brief Copies service statistics into caller-owned storage. */
void BootUpdateServiceGetStats(boot_update_service_stats_t* stats);

/** @brief Consumes a validated jump request after its response has been transmitted. */
bool BootUpdateServiceTakeJumpRequest(void);

#endif
