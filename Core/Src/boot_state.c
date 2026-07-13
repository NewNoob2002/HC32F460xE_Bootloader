#include "boot_state.h"
#include "app_validator.h"
#include "boot_app.h"
#include "boot_config.h"
#include "bsp_clock.h"
#include "bsp_gpio.h"
#include "bsp_reset.h"
#include "bsp_watchdog.h"
void boot_early_init(boot_context_t* context) {
    context->reset_flags = bsp_reset_capture_and_clear();
    bsp_gpio_safe_init();
    bsp_clock_init();
    bsp_watchdog_init();
    context->timeout_polls = 0U;
    context->jump_requested = false;
}
void boot_select_mode(boot_context_t* context) {
    boot_app_select_mode(context, boot_application_vector_is_valid(), bsp_reset_was_software(context->reset_flags));
}
void boot_timeout_poll(boot_context_t* context) {
    if ((context->mode == BOOT_MODE_UPDATE_WINDOW) && (++context->timeout_polls >= BOOT_UPDATE_WINDOW_POLLS))
        context->jump_requested = true;
}
