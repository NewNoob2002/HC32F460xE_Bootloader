#include "message_decode.h"
#include "core_log.h"
#include "liteParse.h"
#include "mcu_config.h"
#include "delay.h"

extern ledState_t powerLedState;
extern ledState_t functionLedState;
extern uint32_t BOOT_DELAY_TIME;
extern uint32_t u32FlashAddr_Erase;

int message_decode(Parser16 *parse, uint8_t *txBuffer)
{
    DDL_ASSERT(parse != NULL);
    DDL_ASSERT(txBuffer != NULL);
    uint16_t cmd_id = parse->buffer[PACKET_CMD_INDEX];

    uint16_t data_length = parse->payload_len - PACKET_INSTRUCT_SEGMENT_SIZE;
    uint32_t u32FlashAddr = *((uint32_t *)&parse->buffer[PACKET_ADDRESS_INDEX]);
    bool flash_addr_valid = false;
    int ret = 0;
    static int step = 0;

    if (PACKET_CMD_TYPE_DATA == parse->buffer[PACKET_TYPE_INDEX])
    {
        if ((u32FlashAddr >= (EFM_BASE + BOOT_SIZE)) && (u32FlashAddr < (EFM_BASE + EFM_END_ADDR)))
        {
            LOG_INFO("step %d Start Addr:%08x\n", ++step, u32FlashAddr);
            flash_addr_valid = true;
        }
        else
        {
            LOG_INFO("Addr Error\n");
            flash_addr_valid = false;
        }
    }
    switch (cmd_id)
    {
    case PACKET_CMD_HANDSHAKE: {
        functionLedState.currentRate = 1000;
        LOG_INFO("[%d] HANDSHAKE\n", millis());
        memcpy(txBuffer, parse->buffer, (parse->payload_len + FRAME_HEADER_LEN + BOOT_MSG_CRC_LEN));
        txBuffer[PACKET_RESULT_INDEX] = PACKET_ACK_OK;
        uint16_t crc16 = Cal_CRC16(&txBuffer[FRAME_PACKET_INDEX], parse->payload_len);
        txBuffer[FRAME_PACKET_INDEX + parse->payload_len] = crc16 & 0x00FF;
        txBuffer[FRAME_PACKET_INDEX + parse->payload_len + 1] = crc16 >> 8;
        ret = parse->payload_len + FRAME_HEADER_LEN + BOOT_MSG_CRC_LEN;
        break;
    }
    case PACKET_CMD_ERASE_FLASH: {
        functionLedState.currentRate = 500;
        LOG_INFO("[%d] ERASE_FLASH\n", millis());
        memcpy(txBuffer, parse->buffer, (PACKET_INSTRUCT_SEGMENT_SIZE + FRAME_HEADER_LEN + BOOT_MSG_CRC_LEN));
        if (flash_addr_valid)
        {
            int32_t i32Ret = LL_ERR;
            //            FlashEraseSector(APP_STATUS_SECTOR);
            uint16_t u16PageNum = FLASH_PageNumber(u32FlashAddr);
						u32FlashAddr_Erase = u32FlashAddr;
            LOG_INFO("ERASE_FLASH STEP, FlashAddr: %08x, PageNum:%d\n", u32FlashAddr, u16PageNum);
            for (uint8_t i = 0; i < u16PageNum; i++)
            {
                i32Ret = FlashEraseSector(BOOT_SIZE / EFM_SECTOR_SIZE + i);

                if (LL_OK != i32Ret)
                {
                    txBuffer[PACKET_RESULT_INDEX] = PACKET_ACK_ERROR;
                    break;
                }
            }
            if (LL_OK == i32Ret)
            {
                txBuffer[PACKET_RESULT_INDEX] = PACKET_ACK_OK;
            }
        }
        else
        {
            txBuffer[PACKET_RESULT_INDEX] = PACKET_ACK_ADDR_ERROR;
        }
        txBuffer[FRAME_LENGTH_INDEX] = PACKET_INSTRUCT_SEGMENT_SIZE;
        uint16_t crc16 = Cal_CRC16(&txBuffer[FRAME_PACKET_INDEX], PACKET_INSTRUCT_SEGMENT_SIZE);
        txBuffer[FRAME_PACKET_INDEX + PACKET_INSTRUCT_SEGMENT_SIZE] = crc16 & 0x00FF;
        txBuffer[FRAME_PACKET_INDEX + PACKET_INSTRUCT_SEGMENT_SIZE + 1] = crc16 >> 8;
        ret = PACKET_INSTRUCT_SEGMENT_SIZE + FRAME_HEADER_LEN + BOOT_MSG_CRC_LEN;
        break;
    }
    case PACKET_CMD_APP_DOWNLOAD: {
        functionLedState.currentRate = 100;
        LOG_INFO("[%d] APP_DOWNLOAD\n", millis());
        memcpy(txBuffer, parse->buffer, (PACKET_INSTRUCT_SEGMENT_SIZE + FRAME_HEADER_LEN + BOOT_MSG_CRC_LEN));
        if (flash_addr_valid)
        {
            LOG_INFO("FlashAddr: %08x, DataLength: %d\n", u32FlashAddr, data_length);
            int32_t i32Ret = FlashWritePage(u32FlashAddr, (uint8_t *)&parse->buffer[PACKET_DATA_INDEX], data_length);
            if (LL_OK != i32Ret)
            {
                txBuffer[PACKET_RESULT_INDEX] = PACKET_ACK_ERROR;
            }
            else
            {
                txBuffer[PACKET_RESULT_INDEX] = PACKET_ACK_OK;
            }
        }
        else
        {
            txBuffer[PACKET_RESULT_INDEX] = PACKET_ACK_ADDR_ERROR;
        }
        txBuffer[FRAME_LENGTH_INDEX] = PACKET_INSTRUCT_SEGMENT_SIZE;
        txBuffer[FRAME_LENGTH_INDEX + 1] = 0;
        uint16_t crc16 = Cal_CRC16(&txBuffer[FRAME_PACKET_INDEX], PACKET_INSTRUCT_SEGMENT_SIZE);
        txBuffer[FRAME_PACKET_INDEX + PACKET_INSTRUCT_SEGMENT_SIZE] = crc16 & 0x00FF;
        txBuffer[FRAME_PACKET_INDEX + PACKET_INSTRUCT_SEGMENT_SIZE + 1] = crc16 >> 8;
        ret = PACKET_INSTRUCT_SEGMENT_SIZE + FRAME_HEADER_LEN + BOOT_MSG_CRC_LEN;
        break;
    }
    case PACKET_CMD_JUMP_TO_APP: {
        LOG_INFO("[%d] JUMP_TO_APP\n", millis());
				uint32_t u32Temp = APP_FLAG;
				FlashWritePage(BOOT_PARA_ADDRESS, (uint8_t *)&u32Temp, 4);
        memcpy(txBuffer, parse->buffer, (parse->payload_len + FRAME_HEADER_LEN + BOOT_MSG_CRC_LEN));
        txBuffer[PACKET_RESULT_INDEX] = PACKET_ACK_OK;
        uint16_t crc16 = Cal_CRC16(&txBuffer[FRAME_PACKET_INDEX], parse->payload_len);
        txBuffer[FRAME_PACKET_INDEX + parse->payload_len] = crc16 & 0x00FF;
        txBuffer[FRAME_PACKET_INDEX + parse->payload_len + 1] = crc16 >> 8;
        ret = parse->payload_len + FRAME_HEADER_LEN + BOOT_MSG_CRC_LEN;
				BOOT_DELAY_TIME = 50;
        break;
    }
    default:
        break;
    }

    return ret;
}
