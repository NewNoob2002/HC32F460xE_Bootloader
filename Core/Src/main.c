#include "main.h"
#include "app_jump.h"
#include "app_validator.h"
#include "boot_state.h"
#include "boot_timebase.h"
#include "bsp_board_config.h"
#include "bsp_clock.h"
#include "bsp_debug_port.h"
#include "bsp_external_watchdog.h"
#include "bsp_power.h"
#include "bsp_status_led.h"
#include "bsp_write_protection.h"
#include "elog.h"

#if BOOT_ENABLE_EASYLOGGER
static const char* mode_name(boot_mode_t mode) {
    if (mode == BOOT_MODE_START_APPLICATION)
        return "app";
    if (mode == BOOT_MODE_UPDATE_WINDOW)
        return "update";
    return "recovery";
}
#endif
static void prepare_for_application(void) {
    bsp_status_led_off();
    bsp_power_hold_assert();
}
static void fatal_safe_loop(bool timebase_ready) {
    bsp_status_led_set_mode(BOOT_LED_MODE_FATAL);
    while (1) {
        bsp_power_hold_assert();
        if (timebase_ready) {
            uint32_t now = boot_time_ms();
            bsp_external_watchdog_poll(now);
            bsp_status_led_poll(now);
        }
    }
}
int main(void) {
#ifdef BOOT_LOG_DEBUG_BUILD
    elog_init();
    elog_set_fmt(ELOG_LVL_ASSERT, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME | ELOG_FMT_FUNC | ELOG_FMT_LINE);
    elog_set_fmt(ELOG_LVL_ERROR, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME | ELOG_FMT_FUNC | ELOG_FMT_LINE);
    elog_set_fmt(ELOG_LVL_WARN, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME);
    elog_set_fmt(ELOG_LVL_INFO, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME);
    elog_set_fmt(ELOG_LVL_DEBUG, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME);
    elog_start();
#endif

    boot_context_t context;
    bsp_write_protection_unlock();
    boot_capture_reset_info(&context);
    bsp_power_init();
    bsp_debug_port_release_watchdog_pin();
    bsp_clock_init();
    if (!boot_timebase_init())
        fatal_safe_loop(false);
    context.watchdog_ready = bsp_external_watchdog_init();
    context.led_ready = bsp_status_led_init();
    context.log_ready = true;
    bsp_write_protection_restore();
    context.app_valid = boot_application_vector_is_valid();
    context.update_started_ms = boot_time_ms();
    boot_select_mode(&context);
    log_i("id=%s reset=%08lx app=%u mode=%s", BOOT_BUILD_ID, (unsigned long)context.reset_info.raw_flags,
          context.app_valid ? 1U : 0U, mode_name(context.mode));
    log_i("hold=%u", bsp_power_hold_is_asserted() ? 1U : 0U);
    log_i("por=%u pin=%u sw=%u wdt=%u lowv=%u clk=%u multi=%u", context.reset_info.power_on_reset ? 1U : 0U,
          context.reset_info.pin_reset ? 1U : 0U, context.reset_info.software_reset ? 1U : 0U,
          context.reset_info.watchdog_reset ? 1U : 0U, context.reset_info.low_voltage_reset ? 1U : 0U,
          context.reset_info.clock_failure_reset ? 1U : 0U, context.reset_info.multiple_reset_sources ? 1U : 0U);
    log_i("flash=%08lx+%08lx app_base=%08lx wdog=%u led=%u", (unsigned long)BOOT_FLASH_BASE,
          (unsigned long)BOOT_FLASH_SIZE, (unsigned long)APP_FLASH_BASE, context.watchdog_ready ? 1U : 0U,
          context.led_ready ? 1U : 0U);
    if (context.mode == BOOT_MODE_START_APPLICATION) {
        prepare_for_application();
        (void)boot_jump_to_application(APP_FLASH_BASE);
        fatal_safe_loop(true);
    }
    bsp_status_led_set_mode(context.mode == BOOT_MODE_UPDATE_WINDOW ? BOOT_LED_MODE_UPDATE_WINDOW
                                                                    : BOOT_LED_MODE_RECOVERY);
    while (1) {
        uint32_t now_ms = boot_time_ms();
        bsp_external_watchdog_poll(now_ms);
        bsp_status_led_poll(now_ms);
        boot_timeout_poll(&context, now_ms);
        if (context.jump_requested) {
            prepare_for_application();
            (void)boot_jump_to_application(APP_FLASH_BASE);
            fatal_safe_loop(true);
        }
    }
}
