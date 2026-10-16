#define LOG_TAG "App"
#include "app_runtime.h"
#include "app_battery.h"
#include "app_keys.h"
#include "app_led.h"
#include "app_panel.h"
#include "app_param_store.h"
#include "app_power.h"
#include "app_recording.h"
#include "bsp_board_config.h"
#include "bsp_critical.h"
#include "bsp_debug_port.h"
#include "bsp_external_watchdog.h"
#include "bsp_indicator_led.h"
#include "bsp_power.h"
#include "bsp_reset.h"
#include "bsp_status_led.h"
#include "bsp_timebase.h"
#include "bsp_write_protection.h"
#include "firmware_log.h"
#include "hc32_ll.h"

namespace {
uint32_t heartbeat_ms;
uint32_t last_led_ms;
bool led_started;
AppLedChannel power_led{}, charge_led{}, function_led{};

void update_leds(uint32_t now_ms) {
    const auto battery = app_battery_snapshot(now_ms);
    const auto pattern = app_led_select(
        {app_power_snapshot().state, app_panel_linux_seen(), app_recording_snapshot().active, battery.valid,
         battery.charger_valid && (battery.charge_state == 1U || battery.charge_state == 2U),
         battery.charger_valid && (battery.charge_state == 2U), battery.charger_valid && (battery.charge_state == 3U),
         battery.percent});
    if (led_started && (power_led.mode == pattern.power) && (charge_led.mode == pattern.charge)
        && (function_led.mode == pattern.function) && !bsp_time_elapsed(now_ms, last_led_ms, 100U))
        return;
    led_started = true;
    last_led_ms = now_ms;
    bsp_indicator_led_write(BSP_INDICATOR_LED_POWER, app_led_level(power_led, pattern.power, now_ms));
    bsp_indicator_led_write(BSP_INDICATOR_LED_CHARGE, app_led_level(charge_led, pattern.charge, now_ms));
    bsp_status_led_write(app_led_level(function_led, pattern.function, now_ms));
}

bool logging_init() {
#if FIRMWARE_CFG_LOGGING
    if (elog_init() != ELOG_NO_ERR)
        return false;
    elog_output_lock_enabled(false);
    for (uint8_t level = ELOG_LVL_ASSERT; level <= ELOG_LVL_DEBUG; ++level)
        elog_set_fmt(level,
                     static_cast<size_t>(static_cast<uint32_t>(ELOG_FMT_LVL) | static_cast<uint32_t>(ELOG_FMT_TAG)
                                         | static_cast<uint32_t>(ELOG_FMT_TIME)));
    elog_start();
#endif
    return true;
}

bool initialize_hardware() {
    if (!bsp_power_init() || !bsp_power_hold_is_asserted())
        return false;
    bsp_debug_port_configure_for_gpio();
    // Boot 已运行在 MPLL；交接时验证并继承，不重写正在使用的 PLL。
    SystemCoreClockUpdate();
    if ((SystemCoreClock != 200000000UL) || (CLK_GetStableStatus(CLK_STB_FLAG_PLL) != SET)
        || (CLK_GetStableStatus(CLK_STB_FLAG_XTAL) != SET))
        return false;
    if (!bsp_timebase_init() || !bsp_status_led_init() || !bsp_indicator_led_init()
        || !bsp_external_watchdog_init(bsp_time_ms()) || !logging_init())
        return false;
    uint8_t stored_offset = 0U;
    if (!app_param_store_load(stored_offset)) {
        stored_offset = 0U;
        FIRMWARE_LOG_WARN("SOC offset load failed; using 0");
    }
    return app_battery_init(stored_offset) && app_panel_init() && app_keys_init(bsp_time_ms());
}
} // namespace

volatile AppRuntimeDiagnostics app_runtime_diagnostics;

bool app_runtime_init() {
    power_led = charge_led = function_led = {};
    led_started = false;
    app_runtime_diagnostics.poll_count = 0U;
    app_runtime_diagnostics.last_poll_ms = 0U;
    app_runtime_diagnostics.max_poll_gap_ms = 0U;
    app_runtime_diagnostics.max_work_ms = 0U;
    (void)bsp_enter_critical();
    bsp_write_protection_unlock();
    const bool ready = initialize_hardware();
    bsp_write_protection_restore();
    if (ready) {
        heartbeat_ms = bsp_time_ms();
        // 复位标志会累积；只有纯上电标志才执行冷启动按键确认，其他复位直接运行。
        app_power_init(bsp_reset_capture() == RMU_FLAG_PWR_ON, heartbeat_ms);
        // Boot 留下 PRIMASK=1；仅在 App 状态和 SysTick 就绪后显式恢复到 0。
        bsp_exit_critical(0U);
        FIRMWARE_LOG_INFO("id=%s product=%s ready clock=%lu Hz", APP_BUILD_ID, APP_PRODUCT_NAME,
                          static_cast<unsigned long>(SystemCoreClock));
    }
    return ready;
}

void app_runtime_poll(uint32_t now_ms) {
    const uint32_t gap_ms = now_ms - app_runtime_diagnostics.last_poll_ms;
    if ((app_runtime_diagnostics.poll_count != 0U) && (gap_ms > app_runtime_diagnostics.max_poll_gap_ms))
        app_runtime_diagnostics.max_poll_gap_ms = gap_ms;
    app_runtime_diagnostics.last_poll_ms = now_ms;
    ++app_runtime_diagnostics.poll_count;
    // 每轮服务 1 ms 看门狗脉冲；日志按固定周期输出，不阻塞等时。
    bsp_external_watchdog_poll(now_ms);
    app_power_poll(now_ms);
    app_keys_poll(now_ms);
    app_panel_poll(now_ms);
    if (!bsp_external_watchdog_is_pulse_active())
        app_battery_poll(now_ms);
    app_power_poll(bsp_time_ms()); // 新样本/完整 ACK 当轮推进，不在 ISR 执行业务或复位。
    update_leds(bsp_time_ms());
    // I2C2 操作可能跨过喂狗截止时刻；再次推进脉冲，再选择 Flash 写入窗口。
    bsp_external_watchdog_poll(bsp_time_ms());
    const auto power = app_power_snapshot().state;
    if ((power == AppPowerState::Running || power == AppPowerState::Off) && !app_panel_response_pending()
        && !bsp_external_watchdog_is_pulse_active())
        app_battery_save_compensation(bsp_time_ms());
    const uint32_t work_ms = bsp_time_ms() - now_ms;
    if (work_ms > app_runtime_diagnostics.max_work_ms)
        app_runtime_diagnostics.max_work_ms = work_ms;
    if (bsp_time_elapsed(now_ms, heartbeat_ms, 1000U)) {
        heartbeat_ms = now_ms;
        FIRMWARE_LOG_DEBUG("alive t=%lu ms", static_cast<unsigned long>(now_ms));
    }
}
