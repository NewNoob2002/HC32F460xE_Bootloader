#include "bsp_i2c_slave.h"
#include <string.h>
#include "hc32f460.h"

static volatile uint8_t rx_buffer[BSP_I2C_RX_CAPACITY];
static volatile size_t rx_length;
static volatile bool rx_complete;
static volatile bool rx_overflow;
static uint8_t tx_buffer[BSP_I2C_TX_CAPACITY];
static size_t tx_length;

bool bsp_i2c_slave_init(void) {
    /* I2C1 pins and programmable NVIC channels are UNKNOWN in this checkout. */
    rx_length = 0U;
    rx_complete = false;
    rx_overflow = false;
    tx_length = 0U;
    return false;
}
void bsp_i2c_slave_deinit(void) {
    tx_length = 0U;
}
void bsp_i2c_slave_poll(void) {}
void bsp_i2c_slave_isr_receive_byte(uint8_t byte) {
    if (!rx_complete && (rx_length < BSP_I2C_RX_CAPACITY))
        rx_buffer[rx_length++] = byte;
    else
        rx_overflow = true;
}
void bsp_i2c_slave_isr_stop(void) {
    rx_complete = true;
}
void bsp_i2c_slave_isr_error(void) {
    rx_overflow = true;
    rx_complete = true;
}
bool bsp_i2c_slave_take_rx(uint8_t* destination, size_t capacity, size_t* length) {
    size_t count;
    if ((destination == NULL) || (length == NULL) || !rx_complete)
        return false;
    __disable_irq();
    count = rx_length;
    if (!rx_overflow && (count <= capacity)) {
        for (size_t index = 0U; index < count; ++index)
            destination[index] = rx_buffer[index];
    }
    rx_length = 0U;
    rx_complete = false;
    bool valid = !rx_overflow && (count <= capacity);
    rx_overflow = false;
    __enable_irq();
    *length = valid ? count : 0U;
    return valid;
}
bool bsp_i2c_slave_set_tx(const uint8_t* source, size_t length) {
    if ((source == NULL) || (length > sizeof(tx_buffer)))
        return false;
    memcpy(tx_buffer, source, length);
    tx_length = length;
    return true;
}
