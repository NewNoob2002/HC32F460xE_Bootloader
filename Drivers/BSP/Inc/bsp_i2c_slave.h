#ifndef BSP_I2C_SLAVE_H
#define BSP_I2C_SLAVE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define BSP_I2C_RX_CAPACITY 544U
#define BSP_I2C_TX_CAPACITY 544U
typedef struct {
    volatile uint32_t rx_transactions;
    volatile uint32_t tx_complete_reads;
    volatile uint16_t err_count;
} bsp_i2c_slave_counters_t;
bool bsp_i2c_slave_init(void);
void bsp_i2c_slave_deinit(void);
void bsp_i2c_slave_poll(uint8_t* output_buffer, size_t output_buffer_size, size_t* output_length);
void bsp_i2c_slave_get_counters(bsp_i2c_slave_counters_t* counters);

int txBufferAvailable();
uint8_t txBufferRead(void);
int txBufferWrite(uint8_t* buffer, const uint16_t length);
int rxBufferAvailable(void);
uint8_t rxBufferRead(void);
size_t rxBufferReadBytes(uint8_t* buffer, size_t length);
void rxBufferWrite(uint8_t ch);
#endif
