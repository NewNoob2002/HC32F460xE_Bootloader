#include "boot_reset.h"
#include "bsp_reset.h"
#include "hc32_ll.h"

/* Startup excludes .noinit from zeroing; only Boot writes this one-shot token. */
#define UPGRADE_RESET_MAGIC 0x55475044UL
static volatile uint32_t upgrade_reset_marker[2] __attribute__((section(".noinit.upgrade_reset"), used));

void boot_reset_after_upgrade(void) {
    __disable_irq();
    upgrade_reset_marker[0] = UPGRADE_RESET_MAGIC;
    upgrade_reset_marker[1] = (uint32_t)~UPGRADE_RESET_MAGIC;
    __DSB();
    NVIC_SystemReset();
}

bool boot_reset_take_upgrade_marker(uint32_t reset_flags) {
    const bool valid =
        (upgrade_reset_marker[0] == UPGRADE_RESET_MAGIC) && (upgrade_reset_marker[1] == (uint32_t)~UPGRADE_RESET_MAGIC);
    /* Consume even on a non-software reset or an invalid application. */
    upgrade_reset_marker[0] = 0U;
    upgrade_reset_marker[1] = 0U;
    __DSB();
    return valid && bsp_reset_was_software(reset_flags);
}
