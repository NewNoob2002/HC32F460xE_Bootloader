#include "main.h"
#include "app_jump.h"
#include "app_validator.h"
#include "boot_log.h"
#include "boot_state.h"
#include "boot_timebase.h"
#include "bsp_board_config.h"
#include "bsp_clock.h"
#include "bsp_debug_port.h"
#include "bsp_external_watchdog.h"
#include "bsp_power.h"
#include "bsp_status_led.h"
#include "bsp_write_protection.h"

#if BOOT_ENABLE_EASYLOGGER
static const char* mode_name(boot_mode_t mode) {
    if (mode == BOOT_MODE_START_APPLICATION)
        return "app";
    if (mode == BOOT_MODE_UPDATE_WINDOW)
        return "update";
    return "recovery";
}
static bool logging_init(void) {
    if (elog_init() != ELOG_NO_ERR)
        return false;
    elog_output_lock_enabled(false);
    for (uint8_t level = ELOG_LVL_ASSERT; level <= ELOG_LVL_DEBUG; ++level)
        elog_set_fmt(level, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME);
    elog_start();
    return true;
}
#else
static bool logging_init(void) {
    return false;
}
#endif

static void prepare_for_application(void) {
    bsp_status_led_off();
    bsp_external_watchdog_prepare_handover();
    bsp_power_hold_assert();
}

static void fatal_safe_loop(bool timebase_ready) {
    bsp_status_led_set_mode(BOOT_LED_MODE_FATAL);
    while (1) {
        bsp_power_hold_assert();
        if (timebase_ready) {
            uint32_t now_ms = boot_time_ms();
            bsp_external_watchdog_poll(now_ms);
            bsp_status_led_poll(now_ms);
        }
    }
}

int main(void) {
    boot_context_t context;
    uint32_t now_ms;
    bool power_gpio_ready;

    bsp_write_protection_unlock();
    boot_capture_reset_info(&context);
    power_gpio_ready = bsp_power_init();
    bsp_debug_port_configure_for_boot_gpio();
    if (!power_gpio_ready || !bsp_power_hold_is_asserted())
        fatal_safe_loop(false);

    bsp_clock_init();
    if (!boot_timebase_init())
        fatal_safe_loop(false);
    now_ms = boot_time_ms();
    context.watchdog_ready = bsp_external_watchdog_init(now_ms);
    context.led_ready = bsp_status_led_init();
    if (context.led_ready)
        bsp_status_led_set_mode(BOOT_LED_MODE_BOOTING);
    context.log_ready = logging_init();
    bsp_write_protection_restore();

    context.app_valid = boot_application_vector_is_valid();
    context.update_started_ms = boot_time_ms();
    boot_select_mode(&context);

    BOOT_LOG_INFO("PB3 hold=%s", bsp_power_hold_is_asserted() ? "high" : "fault");
    BOOT_LOG_INFO("PSPCR=0x%04x", bsp_debug_port_pspcr());
    BOOT_LOG_INFO("TPL5010 PA6 enabled=%u idle=low pulse=high/%ums interval=%ums", context.watchdog_ready ? 1U : 0U,
                  (unsigned)BOOT_EXTERNAL_WATCHDOG_PULSE_MS, (unsigned)BOOT_EXTERNAL_WATCHDOG_INTERVAL_MS);
    BOOT_LOG_INFO("PB5 active-high enabled=%u", context.led_ready ? 1U : 0U);
    BOOT_LOG_INFO("id=%s reset=%08lx app=%u mode=%s", BOOT_BUILD_ID, (unsigned long)context.reset_info.raw_flags,
                  context.app_valid ? 1U : 0U, mode_name(context.mode));

    if (context.mode == BOOT_MODE_START_APPLICATION) {
        prepare_for_application();
        (void)boot_jump_to_application(APP_FLASH_BASE);
        fatal_safe_loop(true);
    }
    bsp_status_led_set_mode(context.mode == BOOT_MODE_UPDATE_WINDOW ? BOOT_LED_MODE_UPDATE_WINDOW
                                                                    : BOOT_LED_MODE_RECOVERY);
    while (1) {
        now_ms = boot_time_ms();
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
