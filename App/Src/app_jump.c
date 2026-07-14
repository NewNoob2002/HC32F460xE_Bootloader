#include "app_jump.h"
#include "app_validator.h"
#include "boot_memory_map.h"
#include "boot_timebase.h"
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
    SysTick_Suspend();
    msp = *(__IO uint32_t*)app_base;
    reset = *(__IO uint32_t*)(app_base + 4U);
    __DSB();
    __ISB();
    __set_MSP(msp);
    entry = (app_entry_t)reset;
    entry();
    return false;
}
