#include "boot_state.h"
#include "boot_app.h"
#include "boot_config.h"
#include "boot_timebase.h"
#include "bsp_power.h"
#include "bsp_reset.h"
#include "elog.h"
#include "hc32_ll_rmu.h"

void boot_capture_reset_info(boot_context_t* context) {
    uint32_t raw = bsp_reset_capture();
    context->reset_info.raw_flags = raw;
    context->reset_info.power_on_reset = (raw & RMU_FLAG_PWR_ON) != 0U;
    context->reset_info.pin_reset = (raw & RMU_FLAG_PIN) != 0U;
    context->reset_info.software_reset = (raw & RMU_FLAG_SW) != 0U;
    context->reset_info.watchdog_reset = (raw & (RMU_FLAG_WDT | RMU_FLAG_SWDT)) != 0U;
    context->reset_info.low_voltage_reset = (raw & (RMU_FLAG_BROWN_OUT | RMU_FLAG_PVD1 | RMU_FLAG_PVD2)) != 0U;
    context->reset_info.clock_failure_reset = (raw & (RMU_FLAG_CLK_ERR | RMU_FLAG_XTAL_ERR)) != 0U;
    context->reset_info.multiple_reset_sources = (raw & RMU_FLAG_MX) != 0U;
    context->update_started_ms = 0U;
    context->app_valid = false;
    context->watchdog_ready = false;
    context->led_ready = false;
    context->log_ready = false;
    context->jump_requested = false;
}
void boot_select_mode(boot_context_t* context) {
    boot_app_select_mode(context, context->app_valid, context->reset_info.software_reset);
}
void boot_timeout_poll(boot_context_t* context, uint32_t now_ms) {
    if (((uint32_t)(now_ms - context->update_started_ms) >= BOOT_UPDATE_WINDOW_MS)) {
        log_e("BOOT update window expired; power hold deasserted");
        // Wait for 1 second
        while (boot_time_ms() - now_ms < 1000) {
            ;
        }
        bsp_power_hold_deassert();
    }
}
