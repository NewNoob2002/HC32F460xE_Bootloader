#include "bsp_i2c_slave.h"

#include <stddef.h>
#include <string.h>
#include "boot_config.h"
#include "boot_timebase.h"
#include "elog.h"
#include "hc32_ll.h"

#define I2C_UNIT     CM_I2C1
#define I2C_EEI_IRQn INT005_IRQn
#define I2C_RXI_IRQn INT006_IRQn
#define I2C_TEI_IRQn INT004_IRQn

typedef struct {
    uint8_t data[BSP_I2C_RX_CAPACITY];
    volatile uint16_t _BufferHead;
    volatile uint16_t _BufferTail;
} rxtx_transaction_t;

typedef enum { SLAVE_RX = 0, SLAVE_RX_DONE, SLAVE_TX, SLAVE_TX_DONE } i2c_slave_state_t;

static rxtx_transaction_t rx_transactions = {0};
static rxtx_transaction_t tx_transactions = {0};

static volatile i2c_slave_state_t slave_state = SLAVE_RX;
static bsp_i2c_slave_counters_t i2c_slave_counters_stats = {0};

int txBufferAvailable() {
    return tx_transactions._BufferHead - tx_transactions._BufferTail;
}

uint8_t txBufferRead(void) {
    return tx_transactions.data[tx_transactions._BufferTail++];
}

int txBufferWrite(uint8_t* buffer, const uint16_t length) {
    // if the head isn't ahead of the tail, we don't have any characters
    memcpy(tx_transactions.data, buffer, length);
    tx_transactions._BufferHead = length;
    tx_transactions._BufferTail = 0;
    return tx_transactions._BufferHead;
}

int rxBufferAvailable(void) {
    return ((unsigned int)(BSP_I2C_RX_CAPACITY + rx_transactions._BufferHead - rx_transactions._BufferTail))
           % BSP_I2C_RX_CAPACITY;
}

uint8_t rxBufferRead(void) {
    // if the head isn't ahead of the tail, we don't have any characters
    if (rx_transactions._BufferHead == rx_transactions._BufferTail) {
        return 0;
    } else {
        uint8_t c = rx_transactions.data[rx_transactions._BufferTail];
        rx_transactions._BufferTail = (uint16_t)(rx_transactions._BufferTail + 1) % BSP_I2C_RX_CAPACITY;
        return c;
    }
}

size_t rxBufferReadBytes(uint8_t* buffer, size_t length) {
    size_t bytes_read = 0;
    while (bytes_read < length && rx_transactions._BufferHead != rx_transactions._BufferTail) {
        buffer[bytes_read++] = rx_transactions.data[rx_transactions._BufferTail];
        rx_transactions._BufferTail = (uint16_t)(rx_transactions._BufferTail + 1) % BSP_I2C_RX_CAPACITY;
    }
    return bytes_read;
}

void rxBufferWrite(uint8_t ch) {
    uint16_t i = (uint16_t)(rx_transactions._BufferHead + 1) % BSP_I2C_RX_CAPACITY;
    if (i != rx_transactions._BufferTail) {
        rx_transactions.data[rx_transactions._BufferHead] = ch;
        rx_transactions._BufferHead = i;
    }
}

static void i2c_eei_callback(void) {
    if (SET == I2C_GetStatus(I2C_UNIT, I2C_FLAG_MATCH_ADDR0)) {
        I2C_ClearStatus(I2C_UNIT, I2C_CLR_SLADDR0FCLR | I2C_CLR_NACKFCLR | I2C_CLR_STOPFCLR);
        bool transaction_is_read = (SET == I2C_GetStatus(I2C_UNIT, I2C_FLAG_TRA));
        if (transaction_is_read) {
            slave_state = SLAVE_TX;
            I2C_IntCmd(I2C_UNIT, I2C_INT_TX_CPLT, ENABLE);
            if (txBufferAvailable() > 0) {
                I2C_WriteData(I2C_UNIT, txBufferRead());
            }
        } else {
            slave_state = SLAVE_RX;
        }
        I2C_IntCmd(I2C_UNIT, I2C_INT_STOP | I2C_INT_NACK, ENABLE);
    } else if (SET == I2C_GetStatus(I2C_UNIT, I2C_FLAG_NACKF)) {
        I2C_ClearStatus(I2C_UNIT, I2C_CLR_NACKFCLR);
        /* Stop tx or rx process*/
        if (SET == I2C_GetStatus(I2C_UNIT, I2C_FLAG_TRA)) {
            /* Config tx end interrupt function disable*/
            I2C_IntCmd(I2C_UNIT, I2C_INT_TX_CPLT, DISABLE);
            I2C_ClearStatus(I2C_UNIT, I2C_CLR_TENDFCLR);

            /* Read DRR register to release */
            (void)I2C_ReadData(I2C_UNIT);
        } else {
            /* Config rx buffer full interrupt function disable */
            //            I2C_IntCmd(I2C_UNIT, I2C_INT_RX_FULL, DISABLE);
        }
    } else if (SET == I2C_GetStatus(I2C_UNIT, I2C_FLAG_STOP)) {
        /* If stop interrupt occurred */
        /* Disable all interrupt enable flag except SLADDR0IE*/
        I2C_IntCmd(I2C_UNIT, I2C_INT_TX_CPLT | I2C_INT_STOP | I2C_INT_NACK, DISABLE);
        /* Clear STOPF flag */
        I2C_ClearStatus(I2C_UNIT, I2C_CLR_STOPFCLR);
        if (slave_state == SLAVE_RX) {
            slave_state = SLAVE_RX_DONE;
        } else if (slave_state == SLAVE_TX) {
            i2c_slave_counters_stats.err_count = 0;
            tx_transactions._BufferHead = 0;
            slave_state = SLAVE_TX_DONE;
        }
    }
}

static void i2c_rxi_callback(void) {
    rxBufferWrite(I2C_ReadData(I2C_UNIT));
}

static void i2c_tei_callback(void) {
    if ((SET == I2C_GetStatus(I2C_UNIT, I2C_FLAG_TX_CPLT)) && (RESET == I2C_GetStatus(I2C_UNIT, I2C_FLAG_NACKF))) {
        if (txBufferAvailable() > 0) {
            I2C_WriteData(I2C_UNIT, txBufferRead());
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

    FCG_Fcg1PeriphClockCmd(FCG1_PERIPH_I2C1, ENABLE);
    GPIO_SetFunc(GPIO_PORT_A, GPIO_PIN_03, GPIO_FUNC_49);
    GPIO_SetFunc(GPIO_PORT_A, GPIO_PIN_02, GPIO_FUNC_48);
    if (LL_OK != I2C_DeInit(I2C_UNIT) || LL_OK != I2C_StructInit(&config)) {
        log_e("I2c init failed in %s:%d", __FILE__, __LINE__);
        return false;
    }
    config.u32ClockDiv = I2C_CLK_DIV2;
    config.u32Baudrate = 400000U;
    config.u32SclTime = 5UL;
    if (LL_OK != I2C_Init(I2C_UNIT, &config, &baud_error)) {
        log_e("I2c init failed in %s:%d", __FILE__, __LINE__);
        return false;
    }
    I2C_SlaveAddrConfig(I2C_UNIT, I2C_ADDR0, I2C_ADDR_7BIT, BOOT_I2C_SLAVE_ADDRESS);
    if (!register_irq(I2C_EEI_IRQn, INT_SRC_I2C1_EEI, DDL_IRQ_PRIO_09, i2c_eei_callback)
        || !register_irq(I2C_RXI_IRQn, INT_SRC_I2C1_RXI, DDL_IRQ_PRIO_10, i2c_rxi_callback)
        || !register_irq(I2C_TEI_IRQn, INT_SRC_I2C1_TEI, DDL_IRQ_PRIO_10, i2c_tei_callback)) {
        bsp_i2c_slave_deinit();
        log_e("I2c init failed in %s:%d", __FILE__, __LINE__);
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
}

void bsp_i2c_slave_poll(uint8_t* output_buffer, size_t output_buffer_size, size_t* output_length) {
    if (rxBufferAvailable() > 0 && slave_state == SLAVE_RX_DONE) {
        __disable_irq();
        size_t read_bytes = rxBufferReadBytes(output_buffer, output_buffer_size);
        *output_length = read_bytes;
        __enable_irq();
    }
    if (i2c_slave_counters_stats.err_count >= 1000) {
        i2c_slave_counters_stats.err_count = 0;
        bsp_i2c_slave_init();
    }
}

void bsp_i2c_slave_get_counters(bsp_i2c_slave_counters_t* counters) {
    if (counters != NULL)
        *counters = i2c_slave_counters_stats;
}
