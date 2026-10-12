#ifndef BSP_I2C_SLAVE_H
#define BSP_I2C_SLAVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_I2C_RX_CAPACITY 544U
#define BSP_I2C_TX_CAPACITY 544U

typedef enum {
    BSP_I2C_SLAVE_STATE_RX = 0,
    BSP_I2C_SLAVE_STATE_RX_DONE,
    BSP_I2C_SLAVE_STATE_TX,
    BSP_I2C_SLAVE_STATE_TX_DONE
} bsp_i2c_slave_state_t;
typedef enum {
    BSP_I2C_RECOVERY_NONE = 0,
    BSP_I2C_RECOVERY_ACTIVE_STALL,
    BSP_I2C_RECOVERY_HW_BUSY
} bsp_i2c_recovery_reason_t;

typedef struct {
    volatile uint32_t address_match_rx;
    volatile uint32_t address_match_tx;
    volatile uint32_t rx_bytes;
    volatile uint32_t tx_bytes;
    volatile uint32_t stop_events;
    volatile uint32_t nack_events;
    volatile uint32_t arbitration_lost;
    volatile uint32_t rx_overflow;
    volatile uint32_t empty_tx_reads;
    volatile uint32_t response_busy;
    volatile uint32_t rx_transactions;
    volatile uint32_t tx_complete_reads;
    volatile uint32_t recovery_active_stall;
    volatile uint32_t recovery_hw_busy;
    volatile uint32_t recovery_init_failures;
    volatile uint32_t last_sr;
    volatile uint32_t last_recovery_sr;
    volatile bsp_i2c_slave_state_t last_recovery_state;
    volatile bsp_i2c_recovery_reason_t last_recovery_reason;
} bsp_i2c_slave_counters_t;

bool bsp_i2c_slave_init(void);
void bsp_i2c_slave_deinit(void);
bool bsp_i2c_slave_poll(uint32_t now_ms);
size_t bsp_i2c_slave_read(uint8_t* buffer, size_t capacity);
void bsp_i2c_slave_get_counters(bsp_i2c_slave_counters_t* counters);
bsp_i2c_slave_state_t bsp_i2c_slave_get_state(void);

bool bsp_i2c_slave_tx_reserve(void);
void bsp_i2c_slave_tx_cancel_write(void);
int bsp_i2c_slave_tx_available(void);
int bsp_i2c_slave_tx_write(const uint8_t* buffer, uint16_t length);

/* Backward compatibility aliases */
typedef bsp_i2c_slave_state_t i2c_slave_state_t;
#define SLAVE_RX               BSP_I2C_SLAVE_STATE_RX
#define SLAVE_RX_DONE          BSP_I2C_SLAVE_STATE_RX_DONE
#define SLAVE_TX               BSP_I2C_SLAVE_STATE_TX
#define SLAVE_TX_DONE          BSP_I2C_SLAVE_STATE_TX_DONE

#define tx_buffer_reserve      bsp_i2c_slave_tx_reserve
#define tx_buffer_cancel_write bsp_i2c_slave_tx_cancel_write
#define tx_buffer_available    bsp_i2c_slave_tx_available
#define tx_buffer_write        bsp_i2c_slave_tx_write

#ifdef __cplusplus
}
#endif
#endif
