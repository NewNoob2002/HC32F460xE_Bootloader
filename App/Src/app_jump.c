#include "app_jump.h"
#include "app_validator.h"
#include "boot_memory_map.h"
#include "boot_timebase.h"
#include "hc32f460.h"

typedef void (*app_entry_t)(void);

bool boot_jump_to_application(uint32_t app_base) {
    uint32_t msp;
    uint32_t reset;
    app_entry_t entry;
    if ((app_base != APP_FLASH_BASE) || !boot_application_vector_is_valid()) {
        return false;
    }
    msp = *(const volatile uint32_t*)(uintptr_t)app_base;
    reset = *(const volatile uint32_t*)(uintptr_t)(app_base + 4U);
    __disable_irq();
    boot_timebase_deinit();
    for (uint32_t index = 0U; index < 8U; ++index) {
        NVIC->ICER[index] = UINT32_MAX;
        NVIC->ICPR[index] = UINT32_MAX;
    }
    SCB->VTOR = app_base;
    __DSB();
    __ISB();
    __set_MSP(msp);
    entry = (app_entry_t)(uintptr_t)reset;
    entry();
    return false;
}
