#include "main.h"
#include "app_jump.h"
#include "boot_state.h"
#include "bsp_i2c_slave.h"
#include "bsp_watchdog.h"
#include "legacy_protocol.h"
int main(void) {
    boot_context_t context;
    boot_early_init(&context);
    boot_select_mode(&context);
    if (context.mode == BOOT_MODE_START_APPLICATION)
        (void)boot_jump_to_application(APP_FLASH_BASE);
    (void)bsp_i2c_slave_init();
    legacy_protocol_init();
    while (1) {
        bsp_watchdog_service();
        bsp_i2c_slave_poll();
        legacy_protocol_poll();
        boot_timeout_poll(&context);
        if (context.jump_requested)
            (void)boot_jump_to_application(APP_FLASH_BASE);
    }
}
