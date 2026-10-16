#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bsp_i2c_master.h"
#include "hc32_ll.h"

CM_I2C_TypeDef mock_i2c;
static unsigned inits, stops, stage, fail_stage;
static uint32_t ack_state;
static bool address_nack, command_nack, stop_fail;
static int32_t advance(void) {
    return ++stage == fail_stage ? LL_ERR_TIMEOUT : LL_OK;
}
static void reset(void) {
    stage = fail_stage = stops = 0U;
    address_nack = command_nack = stop_fail = false;
    assert(bsp_i2c_master_init());
}
int32_t I2C_DeInit(CM_I2C_TypeDef* unit) {
    memset(unit, 0, sizeof(*unit));
    ++inits;
    return LL_OK;
}
int32_t I2C_StructInit(stc_i2c_init_t* config) {
    memset(config, 0, sizeof(*config));
    return LL_OK;
}
int32_t I2C_Init(CM_I2C_TypeDef* unit, const stc_i2c_init_t* config, float32_t* error) {
    (void)unit;
    assert(config->u32Baudrate == 100000U && config->u32ClockDiv == I2C_CLK_DIV8);
    *error = 0.0F;
    return LL_OK;
}
void I2C_Cmd(CM_I2C_TypeDef* unit, unsigned state) {
    (void)unit;
    assert(state == ENABLE);
}
void I2C_ClearStatus(CM_I2C_TypeDef* unit, uint32_t flags) {
    unit->SR &= ~flags;
}
unsigned I2C_GetStatus(const CM_I2C_TypeDef* unit, uint32_t flags) {
    return (unit->SR & flags) ? SET : RESET;
}
void I2C_AckConfig(CM_I2C_TypeDef* unit, uint32_t ack) {
    (void)unit;
    ack_state = ack;
}
int32_t I2C_Start(CM_I2C_TypeDef* unit, uint32_t timeout) {
    assert(unit == CM_I2C2 && timeout == 65536U);
    return advance();
}
int32_t I2C_Restart(CM_I2C_TypeDef* unit, uint32_t timeout) {
    return I2C_Start(unit, timeout);
}
int32_t I2C_TransAddr(CM_I2C_TypeDef* unit, uint16_t address, uint8_t dir, uint32_t timeout) {
    (void)dir;
    assert(unit == CM_I2C2 && address <= 0x7FU && timeout == 65536U);
    if (address_nack) {
        unit->SR |= I2C_FLAG_NACKF;
        return LL_ERR;
    }
    return advance();
}
int32_t I2C_TransData(CM_I2C_TypeDef* unit, const uint8_t* data, uint32_t size, uint32_t timeout) {
    assert(unit == CM_I2C2 && data != NULL && size <= 2U && timeout == 65536U);
    if (command_nack)
        unit->SR |= I2C_FLAG_NACKF;
    return advance(); // LL原实现可能在数据NACK时仍返回LL_OK。
}
int32_t I2C_MasterReceiveDataAndStop(CM_I2C_TypeDef* unit, uint8_t* data, uint32_t size, uint32_t timeout) {
    assert(unit == CM_I2C2 && size <= 2U && timeout == 65536U);
    data[0] = 0x34U;
    if (size == 2U)
        data[1] = 0x12U;
    return advance();
}
int32_t I2C_Stop(CM_I2C_TypeDef* unit, uint32_t timeout) {
    assert(unit == CM_I2C2 && timeout == 65536U);
    ++stops;
    return stop_fail ? LL_ERR_TIMEOUT : LL_OK;
}
int main(void) {
    uint16_t value = 0xBEEFU;
    reset();
    assert(bsp_i2c_master_read_word(0x0BU, 9U, &value) && value == 0x1234U && stage == 6U && stops == 0U);
    assert(ack_state == I2C_ACK);
    for (unsigned failure = 1U; failure <= 6U; ++failure) {
        reset();
        fail_stage = failure;
        value = 0xBEEFU;
        const unsigned before = inits;
        assert(!bsp_i2c_master_read_word(0x0BU, 9U, &value));
        assert(value == 0xBEEFU && stops == 1U && inits == before + 1U && ack_state == I2C_ACK);
    }
    reset();
    command_nack = true;
    assert(!bsp_i2c_master_read_word(0x0BU, 9U, &value) && stops == 1U);
    reset();
    address_nack = true;
    assert(bsp_i2c_master_probe(0x5CU) == BSP_I2C_PROBE_ADDRESS_NACK);
    reset();
    fail_stage = 2U;
    assert(bsp_i2c_master_probe(0x5CU) == BSP_I2C_PROBE_IO_ERROR);
    reset();
    address_nack = stop_fail = true;
    assert(bsp_i2c_master_probe(0x5CU) == BSP_I2C_PROBE_IO_ERROR);
    reset();
    assert(!bsp_i2c_master_read_word(0x80U, 9U, &value) && stage == 0U);
    assert(!bsp_i2c_master_read_word(0x0BU, 9U, NULL) && stage == 0U);
    puts("i2c_master_tests: bounded stages / NACK classification / output preservation / cleanup PASS");
}
