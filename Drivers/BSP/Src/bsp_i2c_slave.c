#include "bsp_i2c_slave.h"

#include <stddef.h>
#include <string.h>
#include "boot_config.h"
#include "boot_log.h"
#include "bsp_board_config.h"
#include "bsp_critical.h"
#include "hc32_ll.h"

#define I2C_EMPTY_TX_BYTE UINT8_C(0xFF)

static rxtx_transaction_t rx_transactions;
static rxtx_transaction_t tx_transactions;
static volatile i2c_slave_state_t slave_state = SLAVE_RX;
static volatile bool transaction_active;
static volatile bool tx_transaction_has_response;
static volatile uint16_t tx_transaction_start_tail;
static volatile uint16_t tx_transaction_response_length;
static volatile uint16_t tx_transaction_started_bytes;
static volatile uint32_t progress_sequence;
static bool tx_write_reserved;
static bool poll_initialized;
static uint32_t polled_progress_sequence;
static uint32_t last_progress_ms;
static bsp_i2c_slave_counters_t i2c_slave_counters_stats;

static void note_progress(void) {
    ++progress_sequence;
}

int txBufferAvailable(void) {
    return (int)(tx_transactions._BufferHead - tx_transactions._BufferTail);
}

uint8_t txBufferRead(void) {
    if (txBufferAvailable() <= 0)
        return I2C_EMPTY_TX_BYTE;
    return tx_transactions.data[tx_transactions._BufferTail++];
}

bool txBufferReserve(void) {
    const bsp_irq_state_t irq_state = bsp_enter_critical();
    const bool available = !tx_write_reserved && (txBufferAvailable() == 0) && (slave_state != SLAVE_TX);
    if (available)
        tx_write_reserved = true;
    else
        ++i2c_slave_counters_stats.response_busy;
    bsp_exit_critical(irq_state);
    return available;
}

void txBufferCancelWrite(void) {
    const bsp_irq_state_t irq_state = bsp_enter_critical();
    tx_write_reserved = false;
    bsp_exit_critical(irq_state);
}

int txBufferWrite(const uint8_t* buffer, uint16_t length) {
    if ((buffer == NULL) || (length == 0U) || (length > BSP_I2C_TX_CAPACITY)) {
        txBufferCancelWrite();
        return 0;
    }

    const bsp_irq_state_t irq_state = bsp_enter_critical();
    if ((txBufferAvailable() != 0) || (slave_state == SLAVE_TX)) {
        tx_write_reserved = false;
        ++i2c_slave_counters_stats.response_busy;
        bsp_exit_critical(irq_state);
        return 0;
    }
    memcpy(tx_transactions.data, buffer, length);
    tx_transactions._BufferHead = length;
    tx_transactions._BufferTail = 0U;
    tx_write_reserved = false;
    if (!transaction_active)
        slave_state = SLAVE_RX;
    bsp_exit_critical(irq_state);
    return (int)length;
}

int rxBufferAvailable(void) {
    return (int)((BSP_I2C_RX_CAPACITY + rx_transactions._BufferHead - rx_transactions._BufferTail)
                 % BSP_I2C_RX_CAPACITY);
}

uint8_t rxBufferRead(void) {
    if (rx_transactions._BufferHead == rx_transactions._BufferTail)
        return 0U;
    const uint8_t byte = rx_transactions.data[rx_transactions._BufferTail];
    rx_transactions._BufferTail = (uint16_t)(rx_transactions._BufferTail + 1U) % BSP_I2C_RX_CAPACITY;
    return byte;
}

size_t rxBufferReadBytes(uint8_t* buffer, size_t length) {
    size_t bytes_read = 0U;
    if (buffer == NULL)
        return 0U;
    while ((bytes_read < length) && (rx_transactions._BufferHead != rx_transactions._BufferTail)) {
        buffer[bytes_read++] = rx_transactions.data[rx_transactions._BufferTail];
        rx_transactions._BufferTail = (uint16_t)(rx_transactions._BufferTail + 1U) % BSP_I2C_RX_CAPACITY;
    }
    return bytes_read;
}

void rxBufferWrite(uint8_t ch) {
    const uint16_t next = (uint16_t)(rx_transactions._BufferHead + 1U) % BSP_I2C_RX_CAPACITY;
    ++i2c_slave_counters_stats.rx_bytes;
    if (next == rx_transactions._BufferTail) {
        ++i2c_slave_counters_stats.rx_overflow;
        return;
    }
    rx_transactions.data[rx_transactions._BufferHead] = ch;
    rx_transactions._BufferHead = next;
}

static void i2c_queue_next_byte(void) {
    uint8_t byte = I2C_EMPTY_TX_BYTE;
    const bool has_response_byte = tx_transaction_has_response && (txBufferAvailable() > 0);
    if (has_response_byte)
        byte = txBufferRead();
    I2C_WriteData(BOOT_I2C_UNIT, byte);
    ++tx_transaction_started_bytes;
    if (has_response_byte)
        ++i2c_slave_counters_stats.tx_bytes;
    else
        ++i2c_slave_counters_stats.empty_tx_reads;
    note_progress();
}

static void i2c_eei_callback(void) {
    i2c_slave_counters_stats.last_sr = BOOT_I2C_UNIT->SR;

    if (SET == I2C_GetStatus(BOOT_I2C_UNIT, I2C_FLAG_MATCH_ADDR0)) {
        const bool transaction_is_read = SET == I2C_GetStatus(BOOT_I2C_UNIT, I2C_FLAG_TRA);
        transaction_active = true;
        note_progress();
        if (transaction_is_read) {
            ++i2c_slave_counters_stats.address_match_tx;
            slave_state = SLAVE_TX;
            tx_transaction_has_response = txBufferAvailable() > 0;
            tx_transaction_start_tail = tx_transactions._BufferTail;
            tx_transaction_response_length = (uint16_t)txBufferAvailable();
            tx_transaction_started_bytes = 0U;
            i2c_queue_next_byte();
            I2C_IntCmd(BOOT_I2C_UNIT, I2C_INT_TX_CPLT, ENABLE);
        } else {
            ++i2c_slave_counters_stats.address_match_rx;
            slave_state = SLAVE_RX;
        }
        I2C_IntCmd(BOOT_I2C_UNIT, I2C_INT_STOP | I2C_INT_NACK | I2C_INT_ARBITRATE_FAIL, ENABLE);
        I2C_ClearStatus(BOOT_I2C_UNIT, I2C_CLR_SLADDR0FCLR | I2C_CLR_STARTFCLR);
    }

    if (SET == I2C_GetStatus(BOOT_I2C_UNIT, I2C_FLAG_ARBITRATE_FAIL)) {
        ++i2c_slave_counters_stats.arbitration_lost;
        note_progress();
        I2C_IntCmd(BOOT_I2C_UNIT, I2C_INT_TX_CPLT, DISABLE);
        NVIC_ClearPendingIRQ(BOOT_I2C_TEI_IRQn);
        if ((slave_state == SLAVE_TX) && tx_transaction_has_response)
            tx_transactions._BufferTail = tx_transaction_start_tail;
        I2C_ClearStatus(BOOT_I2C_UNIT, I2C_FLAG_CLR_ARBITRATE_FAIL);
    }

    if (SET == I2C_GetStatus(BOOT_I2C_UNIT, I2C_FLAG_NACKF)) {
        ++i2c_slave_counters_stats.nack_events;
        note_progress();
        I2C_ClearStatus(BOOT_I2C_UNIT, I2C_CLR_NACKFCLR);
        if (SET == I2C_GetStatus(BOOT_I2C_UNIT, I2C_FLAG_TRA)) {
            I2C_IntCmd(BOOT_I2C_UNIT, I2C_INT_TX_CPLT, DISABLE);
            NVIC_ClearPendingIRQ(BOOT_I2C_TEI_IRQn);
            I2C_ClearStatus(BOOT_I2C_UNIT, I2C_FLAG_CLR_TX_CPLT);
            (void)I2C_ReadData(BOOT_I2C_UNIT);
        }
    }

    if (SET == I2C_GetStatus(BOOT_I2C_UNIT, I2C_FLAG_STOP)) {
        ++i2c_slave_counters_stats.stop_events;
        note_progress();
        I2C_IntCmd(BOOT_I2C_UNIT,
                   I2C_INT_TX_CPLT | I2C_INT_STOP | I2C_INT_NACK | I2C_INT_ARBITRATE_FAIL, DISABLE);
        NVIC_ClearPendingIRQ(BOOT_I2C_TEI_IRQn);
        I2C_ClearStatus(BOOT_I2C_UNIT, I2C_CLR_STOPFCLR | I2C_CLR_STARTFCLR);
        transaction_active = false;
        if (slave_state == SLAVE_RX) {
            ++i2c_slave_counters_stats.rx_transactions;
            slave_state = SLAVE_RX_DONE;
        } else if (slave_state == SLAVE_TX) {
            if (tx_transaction_has_response
                && (tx_transaction_started_bytes >= tx_transaction_response_length)) {
                ++i2c_slave_counters_stats.tx_complete_reads;
                tx_transactions._BufferHead = 0U;
                tx_transactions._BufferTail = 0U;
                slave_state = SLAVE_TX_DONE;
            } else {
                if (tx_transaction_has_response)
                    tx_transactions._BufferTail = tx_transaction_start_tail;
                slave_state = SLAVE_RX;
            }
            tx_transaction_has_response = false;
        }
    }
}

static void i2c_rxi_callback(void) {
    i2c_slave_counters_stats.last_sr = BOOT_I2C_UNIT->SR;
    rxBufferWrite(I2C_ReadData(BOOT_I2C_UNIT));
    note_progress();
}

static void i2c_tei_callback(void) {
    i2c_slave_counters_stats.last_sr = BOOT_I2C_UNIT->SR;
    if ((SET == I2C_GetStatus(BOOT_I2C_UNIT, I2C_FLAG_TX_CPLT))
        && (RESET == I2C_GetStatus(BOOT_I2C_UNIT, I2C_FLAG_NACKF)))
        i2c_queue_next_byte();
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

    FCG_Fcg1PeriphClockCmd(BOOT_I2C_CLOCK, ENABLE);
    GPIO_SetFunc(BOOT_I2C_SCL_PORT, BOOT_I2C_SCL_PIN, BOOT_I2C_SCL_FUNC);
    GPIO_SetFunc(BOOT_I2C_SDA_PORT, BOOT_I2C_SDA_PIN, BOOT_I2C_SDA_FUNC);
    if ((LL_OK != I2C_DeInit(BOOT_I2C_UNIT)) || (LL_OK != I2C_StructInit(&config))) {
        BOOT_LOG_ERROR("I2c init failed in %s:%d", __FILE__, __LINE__);
        return false;
    }
    config.u32ClockDiv = I2C_CLK_DIV2;
    config.u32Baudrate = BOOT_I2C_BAUDRATE;
    config.u32SclTime = BOOT_I2C_SCL_TIME;
    if (LL_OK != I2C_Init(BOOT_I2C_UNIT, &config, &baud_error)) {
        BOOT_LOG_ERROR("I2c init failed in %s:%d", __FILE__, __LINE__);
        return false;
    }
    I2C_SlaveAddrConfig(BOOT_I2C_UNIT, I2C_ADDR0, I2C_ADDR_7BIT, BOOT_I2C_SLAVE_ADDRESS);
    if (!register_irq(BOOT_I2C_EEI_IRQn, BOOT_I2C_EEI_SOURCE, BOOT_I2C_EEI_PRIORITY, i2c_eei_callback)
        || !register_irq(BOOT_I2C_RXI_IRQn, BOOT_I2C_RXI_SOURCE, BOOT_I2C_RXI_PRIORITY, i2c_rxi_callback)
        || !register_irq(BOOT_I2C_TEI_IRQn, BOOT_I2C_TEI_SOURCE, BOOT_I2C_TEI_PRIORITY, i2c_tei_callback)) {
        bsp_i2c_slave_deinit();
        BOOT_LOG_ERROR("I2c init failed in %s:%d", __FILE__, __LINE__);
        return false;
    }
    memset(&rx_transactions, 0, sizeof(rx_transactions));
    slave_state = SLAVE_RX;
    transaction_active = false;
    tx_transaction_has_response = false;
    tx_transaction_start_tail = 0U;
    tx_transaction_response_length = 0U;
    tx_transaction_started_bytes = 0U;
    tx_write_reserved = false;
    poll_initialized = false;
    I2C_Cmd(BOOT_I2C_UNIT, ENABLE);
    I2C_IntCmd(BOOT_I2C_UNIT, I2C_INT_MATCH_ADDR0 | I2C_INT_RX_FULL, ENABLE);
    return true;
}

void bsp_i2c_slave_deinit(void) {
    NVIC_DisableIRQ(BOOT_I2C_EEI_IRQn);
    NVIC_DisableIRQ(BOOT_I2C_RXI_IRQn);
    NVIC_DisableIRQ(BOOT_I2C_TEI_IRQn);
    NVIC_ClearPendingIRQ(BOOT_I2C_EEI_IRQn);
    NVIC_ClearPendingIRQ(BOOT_I2C_RXI_IRQn);
    NVIC_ClearPendingIRQ(BOOT_I2C_TEI_IRQn);
    (void)INTC_IrqSignOut(BOOT_I2C_EEI_IRQn);
    (void)INTC_IrqSignOut(BOOT_I2C_RXI_IRQn);
    (void)INTC_IrqSignOut(BOOT_I2C_TEI_IRQn);
    (void)I2C_DeInit(BOOT_I2C_UNIT);
    transaction_active = false;
    tx_transaction_has_response = false;
    tx_write_reserved = false;
}

bool bsp_i2c_slave_poll(uint32_t now_ms) {
    const uint32_t sequence = progress_sequence;
    const bool active = transaction_active;
    const bool hardware_busy = SET == I2C_GetStatus(BOOT_I2C_UNIT, I2C_FLAG_BUSY);

    if (!poll_initialized) {
        poll_initialized = true;
        polled_progress_sequence = sequence;
        last_progress_ms = now_ms;
        return false;
    }
    if (sequence != polled_progress_sequence) {
        polled_progress_sequence = sequence;
        last_progress_ms = now_ms;
    }
    if (!active && !hardware_busy) {
        last_progress_ms = now_ms;
        return false;
    }
    if ((uint32_t)(now_ms - last_progress_ms) < BOOT_I2C_STALL_TIMEOUT_MS)
        return false;

    i2c_slave_counters_stats.last_recovery_sr = BOOT_I2C_UNIT->SR;
    i2c_slave_counters_stats.last_recovery_state = slave_state;
    if (active) {
        i2c_slave_counters_stats.last_recovery_reason = BSP_I2C_RECOVERY_ACTIVE_STALL;
        ++i2c_slave_counters_stats.recovery_active_stall;
    } else {
        i2c_slave_counters_stats.last_recovery_reason = BSP_I2C_RECOVERY_HW_BUSY;
        ++i2c_slave_counters_stats.recovery_hw_busy;
    }

    if (tx_transaction_has_response)
        tx_transactions._BufferTail = tx_transaction_start_tail;
    bsp_i2c_slave_deinit();
    if (!bsp_i2c_slave_init())
        ++i2c_slave_counters_stats.recovery_init_failures;
    return true;
}

void bsp_i2c_slave_get_counters(bsp_i2c_slave_counters_t* counters) {
    if (counters == NULL)
        return;
    // ponytail: diagnostic snapshot may span IRQ updates; use a sequence lock if coherent telemetry becomes required.
    *counters = i2c_slave_counters_stats;
}

i2c_slave_state_t bsp_i2c_slave_get_state(void) {
    return slave_state;
}
