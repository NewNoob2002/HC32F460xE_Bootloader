#include "boot_app.h"
#include "boot_config.h"
#include "boot_log.h"
#include "bsp_power.h"
#include "bsp_reset.h"
#include "bsp_status_led.h"
void boot_app_select_mode(boot_context_t* context, bool app_valid, bool software_reset, bool upgrade_completed) {
    if (!app_valid)
        context->mode = BOOT_MODE_RECOVERY;
    else if (software_reset && !upgrade_completed)
        context->mode = BOOT_MODE_UPDATE_WINDOW;
    else
        context->mode = BOOT_MODE_START_APPLICATION;
}

bool boot_timeout_poll(boot_context_t* context, uint32_t now_ms) {
    if (context->mode == BOOT_MODE_START_APPLICATION)
        return false;
    if ((context->mode == BOOT_MODE_UPDATE_WINDOW) && !context->update_request_received
        && (uint32_t)(now_ms - context->update_started_ms) >= BOOT_UPDATE_FIRST_PACKET_TIMEOUT_MS) {
        BOOT_LOG_WARN("BOOT no update request within 1 s; resetting to application");
        bsp_status_led_off();
        bsp_reset_after_upgrade();
        return true;
    }
    if (!context->shutdown_pending && (uint32_t)(now_ms - context->update_started_ms) >= BOOT_UPDATE_WINDOW_MS) {
        context->shutdown_pending = true;
        context->shutdown_started_ms = now_ms;
        bsp_status_led_set_mode(BOOT_LED_MODE_FATAL);
        BOOT_LOG_WARN("BOOT update inactivity timeout; shutdown pending");
    }
    if (context->shutdown_pending && !context->power_released
        && (uint32_t)(now_ms - context->shutdown_started_ms) >= BOOT_SHUTDOWN_DELAY_MS) {
        context->power_released = true;
        bsp_status_led_off();
        bsp_power_hold_deassert();
    }
    return context->shutdown_pending;
}
