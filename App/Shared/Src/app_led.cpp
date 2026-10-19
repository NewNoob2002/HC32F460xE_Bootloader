#include "app_led.h"

namespace {
uint32_t toggle_interval_ms(AppLedMode mode) {
    switch (mode) {
        case AppLedMode::Toggle300Ms:
            return 300U;
        case AppLedMode::Toggle500Ms:
            return 500U;
        case AppLedMode::Toggle1000Ms:
            return 1000U;
        case AppLedMode::Toggle3000Ms:
            return 3000U;
        default:
            return 0U;
    }
}
} // namespace

AppLedPattern app_led_select(const AppLedInput& input) {
    // 旧 X1：开机后功能灯常亮，直到 Linux 出现有效通信，此后按录制状态闪烁或熄灭。
    AppLedMode recording = AppLedMode::On;
    if (input.linux_seen)
        recording = input.recording ? AppLedMode::Toggle500Ms : AppLedMode::Off;
    const bool charge_row = input.charging || input.charge_done;
    AppLedMode charge = AppLedMode::Off;
    if (input.charge_done || (input.charging && input.battery_valid && (input.percent >= 100U)))
        charge = AppLedMode::On;
    else if (input.charging)
        charge = input.fast_charging ? AppLedMode::Toggle300Ms : AppLedMode::Toggle1000Ms;

    if ((input.power == AppPowerState::ShutdownWait) || (input.power == AppPowerState::ShutdownDelay))
        return {AppLedMode::On, AppLedMode::Off, AppLedMode::On};
    if ((input.power == AppPowerState::Off) || (input.power == AppPowerState::Starting))
        return {AppLedMode::Off, charge, AppLedMode::Off};
    if (charge_row)
        return {AppLedMode::Off, charge, recording};
    if (!input.battery_valid)
        return {AppLedMode::Toggle3000Ms, AppLedMode::Off, recording};
    if (input.percent >= 14U)
        return {AppLedMode::On, AppLedMode::Off, recording};
    if (input.percent >= 10U)
        return {AppLedMode::Toggle1000Ms, AppLedMode::Off, recording};
    return {AppLedMode::Toggle300Ms, AppLedMode::Off, recording};
}

bool app_led_level(AppLedChannel& channel, AppLedMode mode, uint32_t now_ms) {
    if (channel.mode != mode) {
        channel.mode = mode;
        channel.mode_started_ms = now_ms;
    }
    if (mode == AppLedMode::On)
        return true;
    const uint32_t interval = toggle_interval_ms(mode);
    if (interval == 0U)
        return false;
    // 每次调用把起点前移到一个周期内：elapsed 始终小于 2 个间隔，不会因 2^32 回绕而错相。
    const uint32_t period = 2U * interval;
    const uint32_t elapsed = (now_ms - channel.mode_started_ms) % period;
    channel.mode_started_ms = now_ms - elapsed;
    return elapsed < interval;
}
