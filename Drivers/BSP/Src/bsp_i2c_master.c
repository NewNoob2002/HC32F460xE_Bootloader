#include "bsp_i2c_master.h"
#include <string.h>
#include "hc32_ll.h"

/* LL 高层 timeout 是轮询次数而非 ms；每次 flag 等待有此明确上限，HIL 测量实际最长耗时。 */
#define MASTER_WAIT_POLLS 65536UL
static bool master_ready;

static bool configure_unit(void) {
    stc_i2c_init_t config;
    float32_t error = 0.0F;
    if ((I2C_DeInit(CM_I2C2) != LL_OK) || (I2C_StructInit(&config) != LL_OK))
        return false;
    /* PCLK3=50MHz，100kHz用/8：总宽度62.5，扣除LL开销后低于5-bit高/低计数上限。 */
    config.u32ClockDiv = I2C_CLK_DIV8;
    config.u32Baudrate = 100000U;
    config.u32SclTime = 5U;
    if (I2C_Init(CM_I2C2, &config, &error) != LL_OK)
        return false;
    I2C_Cmd(CM_I2C2, ENABLE);
    return true;
}

bool bsp_i2c_master_init(void) {
    FCG_Fcg1PeriphClockCmd(FCG1_PERIPH_I2C2, ENABLE);
    GPIO_SetFunc(GPIO_PORT_A, GPIO_PIN_09, GPIO_FUNC_51);
    GPIO_SetFunc(GPIO_PORT_A, GPIO_PIN_08, GPIO_FUNC_50);
    master_ready = configure_unit();
    return master_ready;
}

static bool finish_transfer(bool success, bool stopped) {
    if (!stopped && (I2C_Stop(CM_I2C2, MASTER_WAIT_POLLS) != LL_OK))
        success = false;
    I2C_AckConfig(CM_I2C2, I2C_ACK);
    I2C_ClearStatus(CM_I2C2, I2C_FLAG_CLR_ALL);
    if (!success)
        master_ready = configure_unit();
    return success;
}

bsp_i2c_probe_result_t bsp_i2c_master_probe(uint8_t address) {
    int32_t result;
    bool address_nack;
    if (!master_ready || (address > 0x7FU))
        return BSP_I2C_PROBE_IO_ERROR;
    I2C_ClearStatus(CM_I2C2, I2C_FLAG_CLR_ALL);
    if (I2C_Start(CM_I2C2, MASTER_WAIT_POLLS) != LL_OK) {
        (void)finish_transfer(false, false);
        return BSP_I2C_PROBE_IO_ERROR;
    }
    result = I2C_TransAddr(CM_I2C2, address, I2C_DIR_TX, MASTER_WAIT_POLLS);
    address_nack = (result == LL_ERR) && (I2C_GetStatus(CM_I2C2, I2C_FLAG_NACKF) == SET)
                   && (I2C_GetStatus(CM_I2C2, I2C_FLAG_ARBITRATE_FAIL) == RESET);
    if (!finish_transfer((result == LL_OK) || address_nack, false))
        return BSP_I2C_PROBE_IO_ERROR;
    return address_nack ? BSP_I2C_PROBE_ADDRESS_NACK : BSP_I2C_PROBE_PRESENT;
}

bool bsp_i2c_master_read(uint8_t address, uint8_t command, uint8_t* data, uint8_t size) {
    uint8_t received[2] = {0U};
    bool success;
    if (!master_ready || (address > 0x7FU) || (data == NULL) || (size == 0U) || (size > sizeof(received)))
        return false;
    I2C_ClearStatus(CM_I2C2, I2C_FLAG_CLR_ALL);
    I2C_AckConfig(CM_I2C2, size == 1U ? I2C_NACK : I2C_ACK);
    success = (I2C_Start(CM_I2C2, MASTER_WAIT_POLLS) == LL_OK)
              && (I2C_TransAddr(CM_I2C2, address, I2C_DIR_TX, MASTER_WAIT_POLLS) == LL_OK)
              && (I2C_TransData(CM_I2C2, &command, 1U, MASTER_WAIT_POLLS) == LL_OK)
              && (I2C_GetStatus(CM_I2C2, I2C_FLAG_NACKF) == RESET) && (I2C_Restart(CM_I2C2, MASTER_WAIT_POLLS) == LL_OK)
              && (I2C_TransAddr(CM_I2C2, address, I2C_DIR_RX, MASTER_WAIT_POLLS) == LL_OK)
              && (I2C_MasterReceiveDataAndStop(CM_I2C2, received, size, MASTER_WAIT_POLLS) == LL_OK);
    success = finish_transfer(success, success);
    if (success)
        memcpy(data, received, size);
    return success;
}

bool bsp_i2c_master_read_word(uint8_t address, uint8_t command, uint16_t* value) {
    uint8_t bytes[2];
    if ((value == NULL) || !bsp_i2c_master_read(address, command, bytes, sizeof(bytes)))
        return false;
    *value = (uint16_t)((uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U));
    return true;
}

bool bsp_i2c_master_write_byte(uint8_t address, uint8_t command, uint8_t value) {
    const uint8_t bytes[2] = {command, value};
    bool success;
    if (!master_ready || (address > 0x7FU))
        return false;
    I2C_ClearStatus(CM_I2C2, I2C_FLAG_CLR_ALL);
    success = (I2C_Start(CM_I2C2, MASTER_WAIT_POLLS) == LL_OK)
              && (I2C_TransAddr(CM_I2C2, address, I2C_DIR_TX, MASTER_WAIT_POLLS) == LL_OK)
              && (I2C_TransData(CM_I2C2, bytes, sizeof(bytes), MASTER_WAIT_POLLS) == LL_OK)
              && (I2C_GetStatus(CM_I2C2, I2C_FLAG_NACKF) == RESET);
    return finish_transfer(success, false);
}
