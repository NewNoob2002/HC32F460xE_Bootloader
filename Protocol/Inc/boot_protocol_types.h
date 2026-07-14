#ifndef BOOT_PROTOCOL_TYPES_H
#define BOOT_PROTOCOL_TYPES_H

#include <stdint.h>

#define BOOT_PROTOCOL_SYNC_1           0xAAU
#define BOOT_PROTOCOL_SYNC_2           0x44U
#define BOOT_PROTOCOL_SYNC_3           0x18U
#define BOOT_PROTOCOL_HEADER_SIZE      7U
#define BOOT_PROTOCOL_MIN_PAYLOAD_SIZE 12U
#define BOOT_PROTOCOL_MAX_DATA_SIZE    512U
#define BOOT_PROTOCOL_MAX_PAYLOAD_SIZE (BOOT_PROTOCOL_MIN_PAYLOAD_SIZE + BOOT_PROTOCOL_MAX_DATA_SIZE)
#define BOOT_PROTOCOL_CRC_SIZE         2U
#define BOOT_PROTOCOL_MAX_FRAME_SIZE                                                                                   \
    (BOOT_PROTOCOL_HEADER_SIZE + BOOT_PROTOCOL_MAX_PAYLOAD_SIZE + BOOT_PROTOCOL_CRC_SIZE)

typedef struct {
    uint8_t frame_number;
    uint16_t payload_length;
    uint8_t payload[BOOT_PROTOCOL_MAX_PAYLOAD_SIZE];
} boot_protocol_frame_t;

#endif
