#include "main.h"
#include "app_jump.h"
#include "app_validator.h"
#include "boot_log.h"
#include "boot_protocol_parser.h"
#include "boot_state.h"
#include "boot_timebase.h"
#include "boot_update_service.h"
#include "bsp_board_config.h"
#include "bsp_clock.h"
#include "bsp_debug_port.h"
#include "bsp_external_watchdog.h"
#include "bsp_i2c_slave.h"
#include "bsp_power.h"
#include "bsp_status_led.h"
#include "bsp_write_protection.h"

static boot_protocol_parser_t protocol_parser;
static uint32_t protocol_last_byte_ms;
static boot_context_t context = {0};
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
    bsp_i2c_slave_deinit();
}

static void fatal_safe_loop(bool timebase_ready, bool watchdog_ready, bool led_ready) {
    if (led_ready)
        bsp_status_led_set_mode(BOOT_LED_MODE_FATAL);
    while (1) {
        bsp_power_hold_assert();
        if (timebase_ready) {
            uint32_t now_ms = boot_time_ms();
            if (watchdog_ready)
                bsp_external_watchdog_poll(now_ms);
            if (led_ready)
                bsp_status_led_poll(now_ms);
        }
    }
}

int main(void) {
    uint32_t now_ms;

    bsp_write_protection_unlock();
    bsp_debug_port_configure_for_boot_gpio();
    bsp_clock_init();
    boot_timebase_init();
    bsp_power_init();
    boot_capture_reset_info(&context);

    now_ms = boot_time_ms();
    context.watchdog_ready = bsp_external_watchdog_init(now_ms);
    context.led_ready = bsp_status_led_init();
    if (context.led_ready)
        bsp_status_led_set_mode(BOOT_LED_MODE_BOOTING);
    context.log_ready = logging_init();
    context.app_valid = boot_application_vector_is_valid();
    context.update_started_ms = boot_time_ms();
    context.jump_requested = false;
    boot_select_mode(&context);

    BOOT_LOG_INFO("PB3 hold=%s", bsp_power_hold_is_asserted() ? "high" : "fault");
    BOOT_LOG_INFO("PSPCR=0x%04x", bsp_debug_port_pspcr());
    BOOT_LOG_INFO("TPL5010 PA6 enabled=%u idle=low pulse=high/%ums interval=%ums", context.watchdog_ready ? 1U : 0U,
                  (unsigned)BOOT_EXTERNAL_WATCHDOG_PULSE_MS, (unsigned)BOOT_EXTERNAL_WATCHDOG_INTERVAL_MS);
    BOOT_LOG_INFO("PB5 active-high enabled=%u", context.led_ready ? 1U : 0U);
    BOOT_LOG_INFO("id=%s reset=%08lx app=%u mode=%s", BOOT_BUILD_ID, (unsigned long)context.reset_info.raw_flags,
                  context.app_valid ? 1U : 0U, mode_name(context.mode));

    BOOT_LOG_INFO("init begin %ums", (unsigned)(boot_time_ms() - now_ms));
    bool i2c_ready = bsp_i2c_slave_init();
    bsp_write_protection_restore();
    if (context.mode == BOOT_MODE_START_APPLICATION) {
        prepare_for_application();
        (void)boot_jump_to_application(APP_FLASH_BASE);
        fatal_safe_loop(true, context.watchdog_ready, context.led_ready);
    }
    if (!i2c_ready) {
        BOOT_LOG_ERROR("I2C1 slave initialization failed %d", i2c_ready);
        fatal_safe_loop(true, context.watchdog_ready, context.led_ready);
    }
    BOOT_LOG_INFO("ready addr=0x11 baud=400000 mode=%s", mode_name(context.mode));
    BootUpdateServiceInit();
    BootProtocolParserInit(&protocol_parser);
    BootProtocolParserRegisterCallback(&protocol_parser, BootUpdateServiceFrameCallback, NULL);
    protocol_last_byte_ms = boot_time_ms();
    bsp_status_led_set_mode(context.mode == BOOT_MODE_UPDATE_WINDOW ? BOOT_LED_MODE_UPDATE_WINDOW
                                                                    : BOOT_LED_MODE_RECOVERY);
    while (1) {
        now_ms = boot_time_ms();
        BootProtocolParserProcess(&protocol_parser);
        bsp_external_watchdog_poll(now_ms);
        bsp_status_led_poll(now_ms);
        if ((context.mode == BOOT_MODE_UPDATE_WINDOW) && context.app_valid)
            boot_timeout_poll(&context, now_ms);
        if (context.mode == BOOT_MODE_RECOVERY)
            context.jump_requested = false;
        const bool update_jump_requested =
            (bsp_i2c_slave_get_state() == SLAVE_TX_DONE) && BootUpdateServiceTakeJumpRequest();
        if ((context.jump_requested && context.app_valid) || update_jump_requested) {
            bsp_i2c_slave_counters_t i2c_counters;
            bsp_i2c_slave_get_counters(&i2c_counters);
            log_i("JUMP ACK transmitted tx_reads=%lu rx=%lu PB3=%s", (unsigned long)i2c_counters.tx_complete_reads,
                  (unsigned long)i2c_counters.rx_transactions, bsp_power_hold_is_asserted() ? "high" : "fault");
            log_i("JUMP to application requested by %s", update_jump_requested ? "update service" : "user");
            prepare_for_application();
            (void)boot_jump_to_application(APP_FLASH_BASE);
            fatal_safe_loop(true, context.watchdog_ready, context.led_ready);
        }
    }
}
