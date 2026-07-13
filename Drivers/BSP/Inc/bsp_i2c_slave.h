#ifndef BSP_I2C_SLAVE_H
#define BSP_I2C_SLAVE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define BSP_I2C_RX_CAPACITY 528U
#define BSP_I2C_TX_CAPACITY 64U
bool bsp_i2c_slave_init(void);
void bsp_i2c_slave_deinit(void);
void bsp_i2c_slave_poll(void);
void bsp_i2c_slave_isr_receive_byte(uint8_t byte);
void bsp_i2c_slave_isr_stop(void);
void bsp_i2c_slave_isr_error(void);
bool bsp_i2c_slave_take_rx(uint8_t *destination, size_t capacity, size_t *length);
bool bsp_i2c_slave_set_tx(const uint8_t *source, size_t length);
#endif
