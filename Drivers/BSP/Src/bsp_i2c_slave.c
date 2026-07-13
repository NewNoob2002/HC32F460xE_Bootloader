#include "bsp_i2c_slave.h"

#include <string.h>

#include "boot_config.h"
#include "hc32_ll.h"

#define I2C_UNIT     CM_I2C1
#define I2C_EEI_IRQn INT005_IRQn
#define I2C_RXI_IRQn INT006_IRQn
#define I2C_TEI_IRQn INT004_IRQn

typedef struct {
    uint8_t data[BSP_I2C_RX_CAPACITY];
    volatile uint16_t length;
    volatile bool ready;
    volatile bool overflow;
} rx_transaction_t;

static rx_transaction_t rx_transactions[2];
static volatile uint8_t rx_active;
static uint8_t tx_data[BSP_I2C_TX_CAPACITY];
static volatile uint16_t tx_length;
static volatile uint16_t tx_index;
static volatile bool tx_ready;
static volatile bool tx_read_started;
static volatile bool tx_consumed;
static volatile bool transaction_is_read;
static bsp_i2c_slave_counters_t stats;

static void finish_read(void) {
    if (!tx_read_started)
        return;
    if (tx_index >= tx_length) {
        ++stats.tx_complete_reads;
        tx_consumed = true;
    } else {
        ++stats.tx_partial_reads;
    }
    tx_ready = false;
    tx_read_started = false;
    tx_length = 0U;
    tx_index = 0U;
}

static void i2c_eei_callback(void) {
    if (SET == I2C_GetStatus(I2C_UNIT, I2C_FLAG_MATCH_ADDR0)) {
        I2C_ClearStatus(I2C_UNIT, I2C_CLR_SLADDR0FCLR | I2C_CLR_NACKFCLR | I2C_CLR_STOPFCLR);
        transaction_is_read = (SET == I2C_GetStatus(I2C_UNIT, I2C_FLAG_TRA));
        if (transaction_is_read) {
            tx_index = 0U;
            tx_read_started = true;
            I2C_IntCmd(I2C_UNIT, I2C_INT_TX_CPLT, ENABLE);
            if (tx_ready && (tx_index < tx_length))
                I2C_WriteData(I2C_UNIT, tx_data[tx_index++]);
            else {
                ++stats.tx_overreads;
                I2C_WriteData(I2C_UNIT, 0xFFU);
            }
        }
        I2C_IntCmd(I2C_UNIT, I2C_INT_STOP | I2C_INT_NACK, ENABLE);
    } else if (SET == I2C_GetStatus(I2C_UNIT, I2C_FLAG_NACKF)) {
        I2C_ClearStatus(I2C_UNIT, I2C_CLR_NACKFCLR);
        I2C_IntCmd(I2C_UNIT, I2C_INT_TX_CPLT, DISABLE);
        I2C_ClearStatus(I2C_UNIT, I2C_CLR_TENDFCLR);
        if (transaction_is_read)
            finish_read();
    } else if (SET == I2C_GetStatus(I2C_UNIT, I2C_FLAG_STOP)) {
        I2C_IntCmd(I2C_UNIT, I2C_INT_TX_CPLT | I2C_INT_STOP | I2C_INT_NACK, DISABLE);
        I2C_ClearStatus(I2C_UNIT, I2C_CLR_STOPFCLR);
        if (transaction_is_read) {
            finish_read();
        } else {
            rx_transaction_t* active = &rx_transactions[rx_active];
            if (active->length != 0U || active->overflow) {
                active->ready = true;
                ++stats.rx_transactions;
                if (active->overflow)
                    ++stats.rx_overflows;
                uint8_t next = (uint8_t)(rx_active ^ 1U);
                if (!rx_transactions[next].ready) {
                    rx_active = next;
                    rx_transactions[next].length = 0U;
                    rx_transactions[next].overflow = false;
                }
            }
        }
    }
}

static void i2c_rxi_callback(void) {
    uint8_t byte = I2C_ReadData(I2C_UNIT);
    rx_transaction_t* active = &rx_transactions[rx_active];
    if (!active->ready && !active->overflow && (active->length < BSP_I2C_RX_CAPACITY))
        active->data[active->length++] = byte;
    else
        active->overflow = true;
}

static void i2c_tei_callback(void) {
    if ((SET == I2C_GetStatus(I2C_UNIT, I2C_FLAG_TX_CPLT)) && (RESET == I2C_GetStatus(I2C_UNIT, I2C_FLAG_NACKF))) {
        if (tx_ready && (tx_index < tx_length))
            I2C_WriteData(I2C_UNIT, tx_data[tx_index++]);
        else {
            ++stats.tx_overreads;
            I2C_WriteData(I2C_UNIT, 0xFFU);
        }
    }
}

static bool register_irq(IRQn_Type irq, en_int_src_t source, uint32_t priority, func_ptr_t callback) {
    stc_irq_signin_config_t config = {.enIntSrc = source, .enIRQn = irq, .pfnCallback = callback};
    if (LL_OK != INTC_IrqSignIn(&config))
        return false;
    NVIC_ClearPendingIRQ(irq);
    NVIC_SetPriority(irq, priority);
    NVIC_EnableIRQ(irq);
    return true;
}

bool bsp_i2c_slave_init(void) {
    stc_i2c_init_t config;
    float32_t baud_error = 0.0F;
    memset(rx_transactions, 0, sizeof(rx_transactions));
    memset(&stats, 0, sizeof(stats));
    rx_active = 0U;
    tx_length = tx_index = 0U;
    tx_ready = tx_read_started = tx_consumed = false;
    transaction_is_read = false;

    FCG_Fcg1PeriphClockCmd(FCG1_PERIPH_I2C1, ENABLE);
    GPIO_SetFunc(GPIO_PORT_A, GPIO_PIN_03, GPIO_FUNC_49);
    GPIO_SetFunc(GPIO_PORT_A, GPIO_PIN_02, GPIO_FUNC_48);
    if (LL_OK != I2C_DeInit(I2C_UNIT) || LL_OK != I2C_StructInit(&config))
        return false;
    config.u32ClockDiv = I2C_CLK_DIV2;
    config.u32Baudrate = 400000U;
    config.u32SclTime = 5UL;
    if (LL_OK != I2C_Init(I2C_UNIT, &config, &baud_error))
        return false;
    I2C_SlaveAddrConfig(I2C_UNIT, I2C_ADDR0, I2C_ADDR_7BIT, BOOT_I2C_SLAVE_ADDRESS);
    if (!register_irq(I2C_EEI_IRQn, INT_SRC_I2C1_EEI, DDL_IRQ_PRIO_09, i2c_eei_callback)
        || !register_irq(I2C_RXI_IRQn, INT_SRC_I2C1_RXI, DDL_IRQ_PRIO_10, i2c_rxi_callback)
        || !register_irq(I2C_TEI_IRQn, INT_SRC_I2C1_TEI, DDL_IRQ_PRIO_10, i2c_tei_callback)) {
        bsp_i2c_slave_deinit();
        return false;
    }
    I2C_Cmd(I2C_UNIT, ENABLE);
    I2C_IntCmd(I2C_UNIT, I2C_INT_MATCH_ADDR0 | I2C_INT_RX_FULL, ENABLE);
    return true;
}

void bsp_i2c_slave_deinit(void) {
    NVIC_DisableIRQ(I2C_EEI_IRQn);
    NVIC_DisableIRQ(I2C_RXI_IRQn);
    NVIC_DisableIRQ(I2C_TEI_IRQn);
    NVIC_ClearPendingIRQ(I2C_EEI_IRQn);
    NVIC_ClearPendingIRQ(I2C_RXI_IRQn);
    NVIC_ClearPendingIRQ(I2C_TEI_IRQn);
    (void)INTC_IrqSignOut(I2C_EEI_IRQn);
    (void)INTC_IrqSignOut(I2C_RXI_IRQn);
    (void)INTC_IrqSignOut(I2C_TEI_IRQn);
    (void)I2C_DeInit(I2C_UNIT);
    tx_ready = false;
}

void bsp_i2c_slave_poll(void) {}

bool bsp_i2c_slave_take_rx_transaction(uint8_t* destination, size_t capacity, size_t* length) {
    if ((destination == NULL) || (length == NULL))
        return false;
    for (uint8_t index = 0U; index < 2U; ++index) {
        rx_transaction_t* transaction = &rx_transactions[index];
        if (transaction->ready) {
            __disable_irq();
            uint16_t count = transaction->length;
            bool valid = !transaction->overflow && (count <= capacity);
            if (valid)
                memcpy(destination, transaction->data, count);
            transaction->length = 0U;
            transaction->overflow = false;
            transaction->ready = false;
            __enable_irq();
            *length = valid ? count : 0U;
            return valid;
        }
    }
    return false;
}

bool bsp_i2c_slave_publish_response(const uint8_t* source, size_t length) {
    if ((source == NULL) || (length == 0U) || (length > sizeof(tx_data)) || tx_ready)
        return false;
    __disable_irq();
    memcpy(tx_data, source, length);
    tx_length = (uint16_t)length;
    tx_index = 0U;
    tx_consumed = false;
    tx_ready = true;
    __enable_irq();
    return true;
}

bool bsp_i2c_slave_response_ready(void) {
    return tx_ready;
}
bool bsp_i2c_slave_response_consumed(void) {
    bool consumed = tx_consumed;
    tx_consumed = false;
    return consumed;
}
void bsp_i2c_slave_get_counters(bsp_i2c_slave_counters_t* counters) {
    if (counters != NULL)
        *counters = stats;
}
