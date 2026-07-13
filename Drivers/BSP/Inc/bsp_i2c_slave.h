#ifndef BSP_I2C_SLAVE_H
#define BSP_I2C_SLAVE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define BSP_I2C_RX_CAPACITY 544U
#define BSP_I2C_TX_CAPACITY 544U
typedef struct {
    uint32_t rx_transactions;
    uint32_t rx_overflows;
    uint32_t tx_complete_reads;
    uint32_t tx_partial_reads;
    uint32_t tx_overreads;
} bsp_i2c_slave_counters_t;
bool bsp_i2c_slave_init(void);
void bsp_i2c_slave_deinit(void);
void bsp_i2c_slave_poll(void);
void bsp_i2c_slave_isr_receive_byte(uint8_t byte);
void bsp_i2c_slave_isr_stop(void);
void bsp_i2c_slave_isr_error(void);
bool bsp_i2c_slave_take_rx_transaction(uint8_t* destination, size_t capacity, size_t* length);
bool bsp_i2c_slave_publish_response(const uint8_t* source, size_t length);
bool bsp_i2c_slave_response_ready(void);
bool bsp_i2c_slave_response_consumed(void);
void bsp_i2c_slave_get_counters(bsp_i2c_slave_counters_t* counters);
#endif
