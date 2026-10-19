#include "app_power.h"
#include "app_battery.h"
#include "app_recording.h"
#include "bsp_power.h"
#include "bsp_reset.h"
#include "bsp_timebase.h"

namespace {
AppPowerSnapshot power{};
uint32_t started_ms;
uint32_t confirmed_ms;
uint32_t low_battery_ms;
bool low_battery_started;

void release_power(AppPowerReason reason) {
    if (power.state == AppPowerState::Off)
        return;
    power.state = AppPowerState::Off;
    power.reason = reason;
    power.shutdown_requested = true;
    app_recording_set_allowed(false);
    app_recording_stop();
    bsp_power_hold_deassert();
}
void request_shutdown(AppPowerReason reason, uint32_t now_ms) {
    if (power.state != AppPowerState::Running)
        return;
    power = {AppPowerState::ShutdownWait, reason, true};
    started_ms = now_ms;
    app_recording_set_allowed(false);
    app_recording_stop();
}
} // namespace

void app_power_init(bool cold_start, uint32_t now_ms) {
    power = {cold_start ? AppPowerState::Starting : AppPowerState::Running, AppPowerReason::None, false};
    started_ms = now_ms;
    low_battery_started = false;
    app_recording_set_allowed(!cold_start);
}
AppPowerSnapshot app_power_snapshot() {
    return power;
}
void app_power_key_long_press(uint32_t now_ms) {
    if (power.state == AppPowerState::Starting) {
        power.state = AppPowerState::Running;
        app_recording_set_allowed(true);
    } else if (power.state == AppPowerState::Off) {
        // 外部供电驻留时 MCU 不会因按键复位；只有用户长按离开 Off，驻留本身仍不自动重新保持 PB3。
        bsp_power_hold_assert();
        power = {AppPowerState::Running, AppPowerReason::None, false};
        low_battery_started = false;
        app_recording_set_allowed(true);
    } else {
        request_shutdown(AppPowerReason::Key, now_ms);
    }
}
void app_power_force_shutdown() {
    release_power(AppPowerReason::Forced);
}
bool app_power_can_reset() {
    return power.state == AppPowerState::Running;
}
void app_power_request_reset(uint32_t now_ms) {
    if (!app_power_can_reset())
        return;
    power.state = AppPowerState::ResetWait;
    started_ms = now_ms;
    app_recording_set_allowed(false);
}
void app_power_confirm_reset(uint32_t now_ms) {
    if ((power.state == AppPowerState::ResetWait) && !bsp_time_elapsed(now_ms, started_ms, 5000U))
        power.state = AppPowerState::ResetReady;
}
bool app_power_can_confirm_shutdown() {
    return (power.state == AppPowerState::Running) || (power.state == AppPowerState::ShutdownWait)
           || (power.state == AppPowerState::ShutdownDelay);
}
void app_power_confirm_shutdown(uint32_t now_ms) {
    if (!app_power_can_confirm_shutdown() || (power.state == AppPowerState::ShutdownDelay))
        return;
    if (power.state == AppPowerState::Running)
        request_shutdown(AppPowerReason::Linux, now_ms);
    power.state = AppPowerState::ShutdownDelay;
    confirmed_ms = now_ms;
}
void app_power_poll(uint32_t now_ms) {
    if (power.state == AppPowerState::ResetReady) {
        bsp_reset_request();
        return;
    }
    if ((power.state == AppPowerState::ResetWait) && bsp_time_elapsed(now_ms, started_ms, 5000U)) {
        power.state = AppPowerState::Running;
        // 保留面板未读 ACK；随后完整读出也不能再次复位。
    }
    if ((power.state == AppPowerState::Starting) && bsp_time_elapsed(now_ms, started_ms, 10000U))
        release_power(AppPowerReason::StartupTimeout);
    if (((power.state == AppPowerState::ShutdownWait) || (power.state == AppPowerState::ShutdownDelay))
        && (bsp_time_elapsed(now_ms, started_ms, 20000U)
            || ((power.state == AppPowerState::ShutdownDelay) && bsp_time_elapsed(now_ms, confirmed_ms, 8000U))))
        release_power(power.reason);

    const auto battery = app_battery_snapshot(now_ms);
    const bool low =
        battery.valid && battery.charger_valid && (battery.charge_state == 0U) && (battery.sample.soc_percent <= 9U);
    app_recording_set_allowed((power.state == AppPowerState::Running) && !low);
    if (power.state != AppPowerState::Running || !low) {
        low_battery_started = false;
        return;
    }
    app_recording_stop();
    if (!low_battery_started) {
        low_battery_started = true;
        low_battery_ms = now_ms;
    } else if (bsp_time_elapsed(now_ms, low_battery_ms, 180000U)) {
        request_shutdown(AppPowerReason::LowBattery, now_ms);
    }
}
