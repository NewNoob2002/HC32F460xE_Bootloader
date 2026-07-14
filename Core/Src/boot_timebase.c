#include "boot_timebase.h"
#include "bsp_i2c_slave.h"
#include "hc32f460.h"
#include "system_hc32f460.h"
static volatile uint32_t time_ms;

extern bsp_i2c_slave_counters_t i2c_slave_counters_stats;

bool boot_timebase_init(void) {
    time_ms = 0U;
    return (SystemCoreClock >= 1000U) && (SysTick_Config(SystemCoreClock / 1000U) == 0U);
}
uint32_t boot_time_ms(void) {
    return time_ms;
}
void boot_timebase_deinit(void) {
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
}
void SysTick_Handler(void) {
    ++time_ms;
    i2c_slave_counters_stats.err_count++;
}
