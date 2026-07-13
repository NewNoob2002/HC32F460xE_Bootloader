#ifndef PROTOCOL_CRC16_H
#define PROTOCOL_CRC16_H

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    PACKET_CMD_TYPE_CONTROL     = 0x11,
    PACKET_CMD_TYPE_DATA        = 0x12,
} en_packet_cmd_type_t;

typedef enum
{
    PACKET_CMD_HANDSHAKE       = 0x20,
    PACKET_CMD_JUMP_TO_APP     = 0x21,
    PACKET_CMD_APP_DOWNLOAD    = 0x22,
    PACKET_CMD_APP_UPLOAD      = 0x23,
    PACKET_CMD_ERASE_FLASH     = 0x24,
} en_packet_cmd_t;

typedef enum
{
    PACKET_ACK_OK                = 0x00,
    PACKET_ACK_ERROR             = 0x01,
    PACKET_ACK_ABORT             = 0x02,
    PACKET_ACK_TIMEOUT           = 0x03,
    PACKET_ACK_ADDR_ERROR        = 0x04,
} en_packet_status_t;

/* Frame and packet size */
#define PACKET_INSTRUCT_SEGMENT_SIZE        12
#define PACKET_DATA_SEGMENT_SIZE            512
#define PACKET_MIN_SIZE                     PACKET_INSTRUCT_SEGMENT_SIZE
#define PACKET_MAX_SIZE                     (PACKET_DATA_SEGMENT_SIZE + PACKET_INSTRUCT_SEGMENT_SIZE)

#define FRAME_SHELL_SIZE                    0x09///0x07

/* Frame structure defines */
#define FRAME_HEAD_INDEX                    0x00
#define FRAME_NUM_INDEX                     0x03
#define FRAME_XORNUM_INDEX                  0x04
#define FRAME_LENGTH_INDEX                  0x05
#define FRAME_PACKET_INDEX                  0x07

#define FRAME_RECV_TIMEOUT                  5               // ms
#define FRAME_NUM_XOR_BYTE                  0xFF

/* Packet structure defines */
#define PACKET_CMD_INDEX                   (FRAME_PACKET_INDEX + 0x00)
#define PACKET_TYPE_INDEX                  (FRAME_PACKET_INDEX + 0x01)
#define PACKET_RESULT_INDEX                (FRAME_PACKET_INDEX + 0x01)
#define PACKET_ADDRESS_INDEX               (FRAME_PACKET_INDEX + 0x02)
#define PACKET_DATA_INDEX                  (FRAME_PACKET_INDEX + PACKET_INSTRUCT_SEGMENT_SIZE)

#define BOOT_MSG_SYN_BYTE1    0xaa
#define BOOT_MSG_SYN_BYTE2    0x44
#define BOOT_MSG_SYN_BYTE3    0x18

#define BOOT_MSG_CRC_LEN       2
#define FRAME_HEADER_LEN      FRAME_PACKET_INDEX
#define BOOT_MSG_MAX_LEN      (FRAME_HEADER_LEN + PACKET_INSTRUCT_SEGMENT_SIZE + PACKET_DATA_SEGMENT_SIZE + BOOT_MSG_CRC_LEN)

//    |<------ 8 bytes --------> |<------- PacketData --------->|<- 2 bytes   |                                              |             |
//    +----------+--------+----------------+---------+----------+-------------+
//    | Synchron   Frame_Header |  FRAME_LENGTH_INDEX Bytes     |   CRC-16    |
//    |  24 bits     40bits     |                               |   16 bits   |
//    | 0xAA 44 18              |                               |             |
//    +----------+--------+----------------+---------+----------+-------------+
//                              |                               |
//                              |<---------- CRC -------------->|

#ifdef __cplusplus
extern "C" {
#endif
// 缓冲区大小
#define MAX_PACKET_BUF 256 

typedef enum {
    ST_SYNC_1,   // AA
    ST_SYNC_2,   // 44
    ST_SYNC_3,   // 18
    ST_HEADER,   // 读取头部
    ST_PAYLOAD,  // 读取数据
    ST_CRC       // 校验 (2 bytes)
} ProtoState;

typedef struct {
    ProtoState state;
    uint8_t buffer[640];
    uint16_t buffer_length;
    uint16_t payload_len;
    uint16_t payload_remaining;
    uint16_t calc_crc;     // 实时计算的CRC16
    uint16_t recv_crc;     // 接收到的CRC16
    uint8_t crc_cnt;       // CRC读取计数
} Parser16;

typedef void (*PARSER_CRC_CALLBACK)(Parser16 *parser);
void p16_set_crc_callback(PARSER_CRC_CALLBACK callback);
// 初始化
void p16_init(Parser16 *parser);

// 解析字节，返回 true 表示收到完整包
bool p16_parse_byte(Parser16 *parser, uint8_t byte);

uint16_t Cal_CRC16(const uint8_t *p_data, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif