#include "boot_reset.h"
#include "bsp_i2c_slave.h"
#include "bsp_reset.h"
#include "hc32_ll.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

CM_I2C_TypeDef mock_i2c;
CM_RMU_TypeDef mock_rmu;
CM_PWC_TypeDef mock_pwc;
static func_ptr_t irqs[7];
static unsigned software_reset_requests;

int32_t INTC_IrqSignIn(const stc_irq_signin_config_t* config) {
    assert(config->enIRQn < 7U);
    irqs[config->enIRQn] = config->pfnCallback;
    return LL_OK;
}
int32_t INTC_IrqSignOut(IRQn_Type irq) {
    irqs[irq] = NULL;
    return LL_OK;
}
int32_t I2C_DeInit(CM_I2C_TypeDef* unit) {
    memset(unit, 0, sizeof(*unit));
    return LL_OK;
}
int32_t I2C_StructInit(stc_i2c_init_t* config) {
    memset(config, 0, sizeof(*config));
    return LL_OK;
}
int32_t I2C_Init(CM_I2C_TypeDef* unit, const stc_i2c_init_t* config, float32_t* error) {
    (void)unit;
    (void)config;
    *error = 0.0F;
    return LL_OK;
}
void I2C_SlaveAddrConfig(CM_I2C_TypeDef* unit, uint32_t slot, uint32_t mode, uint32_t address) {
    (void)unit;
    (void)slot;
    (void)mode;
    assert(address == 0x11U);
}
void I2C_Cmd(CM_I2C_TypeDef* unit, unsigned state) {
    (void)unit;
    (void)state;
}
void I2C_IntCmd(CM_I2C_TypeDef* unit, uint32_t flags, unsigned state) {
    if (state == ENABLE)
        unit->CR2 |= flags;
    else
        unit->CR2 &= ~flags;
}
unsigned I2C_GetStatus(const CM_I2C_TypeDef* unit, uint32_t flags) {
    return (unit->SR & flags) ? SET : RESET;
}
void I2C_ClearStatus(CM_I2C_TypeDef* unit, uint32_t flags) {
    unit->SR &= ~flags;
}
void I2C_WriteData(CM_I2C_TypeDef* unit, uint8_t byte) {
    unit->DTR = byte;
    unit->SR &= ~I2C_FLAG_TX_CPLT;
}
uint8_t I2C_ReadData(const CM_I2C_TypeDef* unit) {
    return unit->DRR;
}

void NVIC_SystemReset(void) {
    ++software_reset_requests;
}

static void eei(uint32_t flags) {
    mock_i2c.SR = flags;
    irqs[BSP_I2C_EEI_IRQn]();
}
static void read_address(uint32_t stale_flags) {
    eei(I2C_FLAG_MATCH_ADDR0 | I2C_FLAG_TRA | stale_flags);
    assert(bsp_i2c_slave_get_state() == SLAVE_TX);
    assert((mock_i2c.CR2 & I2C_INT_TX_CPLT) != 0U);
    assert(mock_i2c.DTR == 0xAAU);
}
static void ack_byte(void) {
    mock_i2c.SR = I2C_FLAG_TRA | I2C_FLAG_TX_CPLT;
    irqs[BSP_I2C_TEI_IRQn]();
}
static void finish_read(void) {
    read_address(0U);
    ack_byte();
    assert(mock_i2c.DTR == 0x44U);
    ack_byte();
    assert(mock_i2c.DTR == 0x18U);
    /* STOP clears TRA/TENDF before the EEI callback observes the final NACK. */
    eei(I2C_FLAG_NACKF | I2C_FLAG_STOP);
    assert(bsp_i2c_slave_get_state() == SLAVE_TX_DONE);
    assert(tx_buffer_available() == 0);
}

static void test_response_completion(void) {
    const uint8_t response[] = {0xAAU, 0x44U, 0x18U};
    bsp_i2c_slave_counters_t before, after;
    assert(bsp_i2c_slave_init());
    bsp_i2c_slave_get_counters(&before);
    assert(tx_buffer_reserve());
    assert(tx_buffer_write(response, sizeof(response)) == (int)sizeof(response));
    read_address(I2C_FLAG_STOP | I2C_FLAG_NACKF);
    ack_byte();
    ack_byte();
    /* The last byte is queued but has NOT been clocked by the master. */
    eei(I2C_FLAG_STOP);
    assert(bsp_i2c_slave_get_state() != SLAVE_TX_DONE);
    assert(tx_buffer_available() == (int)sizeof(response));
    bsp_i2c_slave_get_counters(&after);
    assert(after.tx_complete_reads == before.tx_complete_reads);

    read_address(0U);
    ack_byte();
    eei(I2C_FLAG_NACKF | I2C_FLAG_STOP);
    assert(bsp_i2c_slave_get_state() != SLAVE_TX_DONE);
    assert(tx_buffer_available() == (int)sizeof(response));
    finish_read();
    bsp_i2c_slave_get_counters(&after);
    assert(after.tx_complete_reads == before.tx_complete_reads + 1U);

    assert(tx_buffer_reserve());
    assert(tx_buffer_write(response, sizeof(response)) == (int)sizeof(response));
    read_address(0U);
    ack_byte();
    ack_byte();
    eei(I2C_FLAG_ARBITRATE_FAIL | I2C_FLAG_NACKF | I2C_FLAG_STOP);
    assert(bsp_i2c_slave_get_state() != SLAVE_TX_DONE);
    assert(tx_buffer_available() == (int)sizeof(response));
    finish_read();
}

static void test_software_reset_handover(void) {
    assert(!boot_reset_take_upgrade_marker(RMU_FLAG_SW));
    mock_rmu.RSTF0 = 2U;
    boot_reset_after_upgrade();
    assert(software_reset_requests == 1U);
    assert(bsp_reset_capture() == 2U); /* No direct writes to reset flags. */
    assert(boot_reset_take_upgrade_marker(RMU_FLAG_SW));
    assert(!boot_reset_take_upgrade_marker(RMU_FLAG_SW));
    boot_reset_after_upgrade();
    assert(!boot_reset_take_upgrade_marker(2U));
    assert(!boot_reset_take_upgrade_marker(RMU_FLAG_SW));
}

int main(void) {
    test_response_completion();
    test_software_reset_handover();
    puts("boot_handover_tests: PASS");
    return 0;
}
