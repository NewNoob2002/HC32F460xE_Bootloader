#ifndef BSP_I2C_SLAVE_H
#define BSP_I2C_SLAVE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define BSP_I2C_RX_CAPACITY 544
#define BSP_I2C_TX_CAPACITY 544
typedef struct {
    volatile uint32_t rx_transactions;
    volatile uint32_t tx_complete_reads;
    volatile uint16_t err_count;
    volatile uint16_t err_deinit_count;
} bsp_i2c_slave_counters_t;

typedef struct {
    uint8_t data[BSP_I2C_RX_CAPACITY];
    volatile uint16_t _BufferHead;
    volatile uint16_t _BufferTail;
} rxtx_transaction_t;

typedef enum { SLAVE_RX = 0, SLAVE_RX_DONE, SLAVE_TX, SLAVE_TX_DONE } i2c_slave_state_t;

bool bsp_i2c_slave_init(void);
void bsp_i2c_slave_deinit(void);
void bsp_i2c_slave_update();
void basp_i2c_slave_err_reset(void);
void bsp_i2c_slave_get_counters(bsp_i2c_slave_counters_t* counters);
i2c_slave_state_t bsp_i2c_slave_get_state(void);

int txBufferAvailable();
uint8_t txBufferRead(void);
int txBufferWrite(uint8_t* buffer, const uint16_t length);
int rxBufferAvailable(void);
uint8_t rxBufferRead(void);
size_t rxBufferReadBytes(uint8_t* buffer, size_t length);
void rxBufferWrite(uint8_t ch);
#endif
