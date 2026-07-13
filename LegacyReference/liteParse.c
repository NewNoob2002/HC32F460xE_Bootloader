#include "liteParse.h"
#include "core_log.h"
#include <stddef.h>


static PARSER_CRC_CALLBACK crc_callback = NULL;


/**
 * @brief  Update CRC16 for input byte
 * @param  crc_in input value
 * @param  input byte
 * @retval None
 */
static unsigned short Update_CRC16(uint16_t crc_in, uint8_t byte)
{
	uint32_t crc = crc_in;
	uint32_t in = byte | 0x100;

	do
	{
		crc <<= 1;
		in <<= 1;
		if (in & 0x100)
			++crc;
		if (crc & 0x10000)
			crc ^= 0x1021;
	}

	while (!(in & 0x10000));

	return crc & 0xffffu;
}

/**
 * @brief  Cal CRC16 for Packet
 * @param  data
 * @param  length
 * @retval None
 */
uint16_t Cal_CRC16(const uint8_t *p_data, uint32_t size)
{
	uint32_t crc = 0;
	const uint8_t *dataEnd = p_data + size;

	while (p_data < dataEnd)
		crc = Update_CRC16(crc, *p_data++);

	crc = Update_CRC16(crc, 0);
	crc = Update_CRC16(crc, 0);

	return crc & 0xffffu;
}

void p16_set_crc_callback(PARSER_CRC_CALLBACK callback) {
    crc_callback = callback;
}

void p16_init(Parser16 *p) {
    p->state = ST_SYNC_1;
    p->buffer_length = 0;
    p->payload_len = 0;
    p->calc_crc = 0xFFFF; // CCITT-FALSE 初始值通常为 0xFFFF
}

bool p16_parse_byte(Parser16 *p, uint8_t byte) {
    bool packet_valid = false;
    p->buffer[p->buffer_length++] = byte;
    switch (p->state) {
        case ST_SYNC_1:
            if (byte == BOOT_MSG_SYN_BYTE1) {
                p->state = ST_SYNC_2;
            } else {
                p16_init(p);
            }
            break;

        case ST_SYNC_2:
            if (byte == BOOT_MSG_SYN_BYTE2) {
                p->state = ST_SYNC_3;
            } else {
                p16_init(p);
            }
            break;

        case ST_SYNC_3:
            if (byte == BOOT_MSG_SYN_BYTE3) {
                p->state = ST_HEADER;
            } else {
                p16_init(p);
            }
            break;
        case ST_HEADER:{
            if(p->buffer_length >= FRAME_HEADER_LEN) {
                if(p->buffer[FRAME_NUM_INDEX] != (p->buffer[FRAME_XORNUM_INDEX] ^ FRAME_NUM_XOR_BYTE)) {
                    p16_init(p);
                    break;
                }
                p->payload_len = *((uint16_t *)&p->buffer[FRAME_LENGTH_INDEX]);
                p->payload_remaining = p->payload_len;
                p->state = ST_PAYLOAD;
            }
            break;
        }
        case ST_PAYLOAD:{
            if(!--p->payload_remaining) {
                p->crc_cnt = 2; // 2 bytes CRC
                p->state = ST_CRC;
                break;
            }
            break;
        }
        case ST_CRC:{
            if(--p->crc_cnt > 0) {
                break;
            }
            else{
                p->recv_crc = *((uint16_t *)&p->buffer[p->buffer_length - 2]);
                p->calc_crc = Cal_CRC16(&p->buffer[FRAME_PACKET_INDEX], p->payload_len);
                if(p->recv_crc == p->calc_crc) {
                    if(crc_callback != NULL) {
                        crc_callback(p);
                        p16_init(p);
                    }
                    else {
                        packet_valid = true;
                    }
                }
                else {
                    LOG_ERROR("CRC error: %04x != %04x", p->recv_crc, p->calc_crc);
                    p16_init(p);
                }
            }
            break;
        }
        default:
            p16_init(p);
            break;
    }
    if (p->buffer_length >= sizeof(p->buffer)) {
        p16_init(p);
    }
    return packet_valid;
}