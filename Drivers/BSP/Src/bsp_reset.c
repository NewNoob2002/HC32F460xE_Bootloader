#include "bsp_reset.h"
#include "hc32_ll.h"
uint32_t bsp_reset_capture(void) {
    /* Preserve the latched reset cause for the application after direct handover. */
    uint32_t flags = READ_REG16_BIT(CM_RMU->RSTF0, RMU_FLAG_ALL);
    return flags;
}
bool bsp_reset_was_software(uint32_t flags) {
    return (flags & RMU_FLAG_SW) != 0U;
}

/* Startup excludes .noinit from zeroing; only Boot writes this one-shot token. */
#define UPGRADE_RESET_MAGIC 0x55475044UL
static volatile uint32_t upgrade_reset_marker[2]
    __attribute__((section(".noinit.upgrade_reset"), used));

void bsp_reset_after_upgrade(void) {
    __disable_irq();
    upgrade_reset_marker[0] = UPGRADE_RESET_MAGIC;
    upgrade_reset_marker[1] = (uint32_t)~UPGRADE_RESET_MAGIC;
    __DSB();
    NVIC_SystemReset();
}

bool bsp_reset_take_upgrade_marker(uint32_t reset_flags) {
    const bool valid = (upgrade_reset_marker[0] == UPGRADE_RESET_MAGIC)
                       && (upgrade_reset_marker[1] == (uint32_t)~UPGRADE_RESET_MAGIC);
    /* Consume even on a non-software reset or an invalid application. */
    upgrade_reset_marker[0] = 0U;
    upgrade_reset_marker[1] = 0U;
    __DSB();
    return valid && bsp_reset_was_software(reset_flags);
}
