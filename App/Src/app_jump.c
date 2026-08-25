#include "app_jump.h"
#include "app_validator.h"
#include "boot_memory_map.h"
#include "boot_timebase.h"
#include "bsp_external_watchdog.h"
#include "bsp_i2c_slave.h"
#include "bsp_power.h"
#include "bsp_status_led.h"
#include "hc32_ll.h"

typedef void (*app_entry_t)(void);

bool boot_jump_to_application(uint32_t app_base) {
    uint32_t msp;
    uint32_t reset;
    app_entry_t entry;
    if ((app_base != APP_FLASH_BASE) || !boot_application_vector_is_valid()) {
        return false;
    }
    __disable_irq();
    bsp_i2c_slave_deinit();
    bsp_status_led_off();
    bsp_external_watchdog_prepare_handover();
    bsp_power_hold_assert();
    boot_timebase_deinit();
    for (uint32_t index = 0U; index < (sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0])); ++index) {
        NVIC->ICER[index] = UINT32_MAX;
        NVIC->ICPR[index] = UINT32_MAX;
    }
    msp = *(__IO uint32_t*)app_base;
    reset = *(__IO uint32_t*)(app_base + 4U);
    SCB->VTOR = app_base;
    __DSB();
    __ISB();
    __set_MSP(msp);
    entry = (app_entry_t)reset;
    entry();
    return false;
}
