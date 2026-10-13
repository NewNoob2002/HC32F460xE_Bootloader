#include <cassert>
#include <cstdio>
#include "app_battery.h"
#include "app_keys.h"
#include "app_power.h"
#include "app_recording.h"
#include "bsp_keys.h"
#include "bsp_power.h"
#include "bsp_reset.h"
#include "hc32_ll.h"

namespace {
uint32_t now;
unsigned releases;
unsigned resets;
bool power_pressed;
bool function_pressed;
bool keys_ready = true;
bool key_gpio_selected;
AppBatterySnapshot battery;

void initialize(bool cold = false, uint32_t time = 0U) {
    now = time;
    releases = resets = 0U;
    power_pressed = function_pressed = false;
    battery = {AppHardware::GpioV13, {3000U, 7800U, 100U}, now, true, true, false, false, 0U, 100U};
    app_recording_init();
    app_power_init(cold, now);
    assert(app_keys_init(now));
}
void advance(uint32_t duration, bool power = false, bool function = false) {
    power_pressed = power;
    function_pressed = function;
    for (uint32_t elapsed = 0U; elapsed < duration; elapsed += 5U) {
        now += 5U;
        battery.sampled_ms = now;
        app_power_poll(now);
        app_keys_poll(now);
        app_power_poll(now);
    }
}
void poll(uint32_t time) {
    now = time;
    battery.sampled_ms = now;
    app_power_poll(now);
}
} // namespace
extern "C" {
int32_t GPIO_StructInit(stc_gpio_init_t* config) {
    *config = {};
    key_gpio_selected = false;
    return LL_OK;
}
void GPIO_SetFunc(uint8_t port, uint16_t pins, uint8_t function) {
    assert(port == GPIO_PORT_A && pins == (GPIO_PIN_00 | GPIO_PIN_15) && function == GPIO_FUNC_0);
    key_gpio_selected = true;
}
int32_t GPIO_Init(uint8_t port, uint16_t pin, const stc_gpio_init_t* config) {
    assert(key_gpio_selected && port == GPIO_PORT_A && config->u16PinDir == PIN_DIR_IN);
    assert(pin == GPIO_PIN_00 || pin == GPIO_PIN_15);
    assert(config->u16PullUp == (pin == GPIO_PIN_00 ? PIN_PU_OFF : PIN_PU_ON));
    return keys_ready ? LL_OK : -1;
}
uint16_t GPIO_ReadInputPins(uint8_t port, uint16_t pin) {
    assert(port == GPIO_PORT_A && (pin == GPIO_PIN_00 || pin == GPIO_PIN_15));
    const bool high = pin == GPIO_PIN_00 ? power_pressed : !function_pressed;
    return high ? PIN_SET : PIN_RESET;
}
void bsp_power_hold_deassert(void) {
    ++releases;
}
void bsp_reset_request(void) {
    ++resets;
}
}
AppBatterySnapshot app_battery_snapshot(uint32_t time) {
    auto result = battery;
    result.valid = result.valid && (uint32_t)(time - result.sampled_ms) < 6000U;
    return result;
}

int main() {
    keys_ready = false;
    assert(!app_keys_init(0U));
    keys_ready = true;
    initialize(true);
    poll(9999U);
    assert(app_power_snapshot().state == AppPowerState::Starting && releases == 0U);
    poll(10000U);
    assert(app_power_snapshot().state == AppPowerState::Off && releases == 1U);
    assert(app_power_snapshot().reason == AppPowerReason::StartupTimeout);
    app_power_key_long_press(now);
    app_power_force_shutdown();
    poll(50000U);
    assert(releases == 1U && resets == 0U && !app_power_can_reset());

    // MultiButton 消抖、冷启动长按只确认一次；仍按住时不能转成关机。
    initialize(true);
    advance(10U, true);
    advance(20U);
    assert(app_power_snapshot().state == AppPowerState::Starting);
    advance(2015U, true);
    assert(app_power_snapshot().state == AppPowerState::Starting);
    advance(5U, true);
    assert(app_power_snapshot().state == AppPowerState::Running);
    advance(4000U, true);
    assert(app_power_snapshot().state == AppPowerState::Running);
    advance(40U);
    advance(2030U, true);
    assert(app_power_snapshot().state == AppPowerState::ShutdownWait);
    const uint32_t requested = now;
    advance(5000U, true);
    app_power_confirm_shutdown(now);
    app_power_confirm_shutdown(now + 1000U); // 重复确认不能延长8 s。
    poll(requested + 12999U);
    assert(releases == 0U);
    poll(requested + 13000U);
    assert(releases == 1U && app_power_snapshot().state == AppPowerState::Off);

    // 单击不切换录制，双击生成一个操作；低电量禁止重新开始。
    initialize();
    advance(40U, false, true);
    advance(400U);
    assert(!app_recording_snapshot().active);
    advance(40U, false, true);
    advance(80U);
    advance(40U, false, true);
    advance(400U);
    assert(app_recording_snapshot().active && app_recording_snapshot().operation_sequence == 1U);
    battery.sample.soc_percent = 0U;
    poll(now);
    assert(!app_recording_snapshot().active && app_recording_snapshot().operation_sequence == 2U);
    app_recording_confirm(1U); // 较旧响应不得确认新停录制操作。
    assert(app_recording_snapshot().operation_pending);
    app_recording_sync(true);
    app_recording_toggle();
    assert(!app_recording_snapshot().active);
    const uint32_t low_started = now;
    poll(low_started + 179999U);
    assert(app_power_snapshot().state == AppPowerState::Running);
    poll(low_started + 180000U);
    assert(app_power_snapshot().reason == AppPowerReason::LowBattery);
    poll(low_started + 199999U);
    assert(releases == 0U);
    poll(low_started + 200000U);
    assert(releases == 1U);

    // 无效/陈旧/充电/未知充电/SOC恢复都清除低电量计时。
    for (unsigned interruption = 0U; interruption < 5U; ++interruption) {
        initialize();
        battery.sample.soc_percent = 9U;
        poll(0U);
        poll(179000U);
        if (interruption == 0U)
            battery.valid = false;
        if (interruption == 1U)
            battery.sampled_ms = now - 6000U;
        if (interruption == 2U)
            battery.charge_state = 2U;
        if (interruption == 3U)
            battery.charger_valid = false;
        if (interruption == 4U)
            battery.sample.soc_percent = 10U;
        app_power_poll(now);
        battery.valid = battery.charger_valid = true;
        battery.charge_state = 0U;
        battery.sample.soc_percent = 9U;
        poll(180000U);
        poll(359999U);
        assert(app_power_snapshot().state == AppPowerState::Running);
        poll(360000U);
        assert(app_power_snapshot().state == AppPowerState::ShutdownWait);
    }

    // ACK读出不等于Linux关机完成；确认后的8 s仍受最初20 s截止时间限制。
    initialize(false, UINT32_MAX - 10000U);
    const uint32_t start = now;
    app_power_key_long_press(start);
    app_power_key_long_press(start + 15000U);
    poll(start + 19000U);
    app_power_confirm_shutdown(now);
    poll(start + 19999U);
    assert(releases == 0U);
    poll(start + 20000U);
    assert(releases == 1U);
    initialize();
    app_power_confirm_shutdown(10U); // Linux主动完成关机也接受。
    poll(8009U);
    assert(releases == 0U);
    poll(8010U);
    assert(releases == 1U);

    // 复位ACK临界时间、重复请求、回绕和强制关机优先级。
    initialize(false, UINT32_MAX - 1000U);
    const uint32_t reset_started = now;
    app_power_request_reset(now);
    app_power_request_reset(now + 1000U);
    poll(reset_started + 4999U);
    assert(resets == 0U && !app_power_can_reset());
    app_power_confirm_reset(now);
    assert(app_power_snapshot().state == AppPowerState::ResetReady);
    poll(now);
    assert(resets == 1U);
    initialize();
    app_power_request_reset(100U);
    app_power_confirm_reset(5100U);
    poll(5100U);
    app_power_confirm_reset(5200U);
    poll(5200U);
    assert(resets == 0U && app_power_snapshot().state == AppPowerState::Running);
    app_power_request_reset(now);
    app_power_force_shutdown();
    app_power_confirm_reset(now);
    poll(now);
    assert(releases == 1U && resets == 0U);

    initialize();
    advance(8020U, false, true);
    assert(releases == 1U && app_power_snapshot().reason == AppPowerReason::Forced);
    advance(10000U, false, true);
    assert(releases == 1U && resets == 0U);
    initialize(false, UINT32_MAX - 1000U);
    advance(2020U, true);
    assert(app_power_snapshot().state == AppPowerState::ShutdownWait);
    std::puts("x1_power_tests: MultiButton / cold start / one-shot holds / shutdown / reset ACK / low battery PASS");
}
