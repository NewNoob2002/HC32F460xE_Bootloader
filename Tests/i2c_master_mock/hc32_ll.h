#ifndef I2C_MASTER_MOCK_H
#define I2C_MASTER_MOCK_H
#include "../hc32_mock/hc32_ll.h"
#define CM_I2C2          (&mock_i2c)
#define FCG1_PERIPH_I2C2 2U
#define LL_ERR           (-1)
#define LL_ERR_TIMEOUT   (-2)
#define I2C_FLAG_CLR_ALL 0xFFFFFFFFU
#define I2C_CLK_DIV8     3U
#define I2C_ACK          0U
#define I2C_NACK         1U
#define I2C_DIR_TX       0U
#define I2C_DIR_RX       1U
void I2C_AckConfig(CM_I2C_TypeDef* unit, uint32_t ack);
int32_t I2C_Start(CM_I2C_TypeDef* unit, uint32_t timeout);
int32_t I2C_Stop(CM_I2C_TypeDef* unit, uint32_t timeout);
int32_t I2C_Restart(CM_I2C_TypeDef* unit, uint32_t timeout);
int32_t I2C_TransAddr(CM_I2C_TypeDef* unit, uint16_t address, uint8_t dir, uint32_t timeout);
int32_t I2C_TransData(CM_I2C_TypeDef* unit, const uint8_t* data, uint32_t size, uint32_t timeout);
int32_t I2C_MasterReceiveDataAndStop(CM_I2C_TypeDef* unit, uint8_t* data, uint32_t size, uint32_t timeout);
#endif
