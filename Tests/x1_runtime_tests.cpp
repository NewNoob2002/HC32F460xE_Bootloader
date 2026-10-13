#include <cstdio>
#include <cstdlib>
#include "app_battery.h"
#include "app_keys.h"
#include "app_panel.h"
#include "app_param_store.h"
#include "app_power.h"
#include "app_recording.h"
#include "app_runtime.h"
#include "bsp_debug_port.h"
#include "bsp_external_watchdog.h"
#include "bsp_indicator_led.h"
#include "bsp_power.h"
#include "bsp_reset.h"
#include "bsp_status_led.h"
#include "bsp_timebase.h"
#include "bsp_write_protection.h"
#include "hc32_ll.h"

namespace {
enum Stage {
    Power = 1,
    Hold,
    Debug,
    Clock,
    Pll,
    Xtal,
    Timebase,
    Led,
    Indicator,
    Watchdog,
    Params,
    Battery,
    Panel,
    Keys
};
unsigned stage, fail_stage;
bool unlocked, irq_masked;
unsigned watchdog_polls, key_polls, power_polls, battery_polls, save_polls, led_writes;
uint32_t reset_flags = RMU_FLAG_PWR_ON;
uint32_t now_ms, battery_duration_ms, save_duration_ms;
uint8_t stored_offset = 5U, loaded_offset;
bool load_fails, pulse_active, pulse_after_battery, shutdown_after_battery, response_pending, linux_seen;
bool power_on, charge_on, function_on;
AppBatterySnapshot battery;
AppPowerSnapshot power;
AppRecordingSnapshot recording;

void check(bool ok) {
    if (!ok)
        std::abort();
}
bool next_stage(Stage expected) {
    check(unlocked && irq_masked);
    ++stage;
    check(stage == static_cast<unsigned>(expected));
    return stage != fail_stage;
}
void reset(unsigned failure = 0U, uint32_t now = 0U) {
    stage = 0U;
    fail_stage = failure;
    unlocked = irq_masked = false;
    watchdog_polls = key_polls = power_polls = battery_polls = save_polls = led_writes = 0U;
    now_ms = now;
    battery_duration_ms = save_duration_ms = 0U;
    loaded_offset = 0xFFU;
    load_fails = pulse_active = pulse_after_battery = shutdown_after_battery = response_pending = linux_seen = false;
    power_on = charge_on = function_on = false;
    battery = {AppHardware::Mp2762V15Plus, {3000U, 7800U, 50U}, now, true, true, true, false, 0U, 50U};
    power = {};
    recording = {};
}
void start_running() {
    reset_flags = 2U;
    reset();
    check(app_runtime_init());
}
void poll(uint32_t time) {
    now_ms = time;
    app_runtime_poll(time);
}
void expect_leds(bool expected_power, bool expected_charge, bool expected_function) {
    check(power_on == expected_power && charge_on == expected_charge && function_on == expected_function);
}
} // namespace

extern "C" {
uint32_t SystemCoreClock;
uint32_t bsp_reset_capture(void) {
    check(irq_masked && !unlocked && stage == Keys);
    return reset_flags;
}
bsp_irq_state_t bsp_enter_critical(void) {
    irq_masked = true;
    return 1U; // Boot 的继承状态。
}
void bsp_exit_critical(bsp_irq_state_t state) {
    check(!unlocked && stage == Keys && state == 0U);
    irq_masked = false;
}
void bsp_write_protection_unlock(void) {
    check(irq_masked);
    unlocked = true;
}
void bsp_write_protection_restore(void) {
    check(irq_masked && unlocked);
    unlocked = false;
}
bool bsp_power_init(void) {
    return next_stage(Power);
}
bool bsp_power_hold_is_asserted(void) {
    return next_stage(Hold);
}
void bsp_debug_port_configure_for_gpio(void) {
    (void)next_stage(Debug);
}
void SystemCoreClockUpdate(void) {
    SystemCoreClock = next_stage(Clock) ? 200000000U : 16000000U;
}
en_flag_status_t CLK_GetStableStatus(uint8_t flag) {
    check(flag == CLK_STB_FLAG_PLL || flag == CLK_STB_FLAG_XTAL);
    return next_stage(flag == CLK_STB_FLAG_PLL ? Pll : Xtal) ? SET : RESET;
}
bool bsp_timebase_init(void) {
    return next_stage(Timebase);
}
uint32_t bsp_time_ms(void) {
    return now_ms;
}
bool bsp_status_led_init(void) {
    return next_stage(Led);
}
bool bsp_indicator_led_init(void) {
    return next_stage(Indicator);
}
void bsp_indicator_led_write(bsp_indicator_led_t led, bool on) {
    check(!irq_masked && !unlocked);
    check(led == BSP_INDICATOR_LED_POWER || led == BSP_INDICATOR_LED_CHARGE);
    if (led == BSP_INDICATOR_LED_POWER)
        power_on = on;
    else
        charge_on = on;
    ++led_writes;
}
void bsp_status_led_write(bool on) {
    check(!irq_masked && !unlocked);
    function_on = on;
    ++led_writes;
}
bool bsp_external_watchdog_init(uint32_t time) {
    check(time == now_ms);
    return next_stage(Watchdog);
}
bool bsp_external_watchdog_is_pulse_active(void) {
    return pulse_active;
}
void bsp_external_watchdog_poll(uint32_t time) {
    check(!irq_masked && !unlocked && time == now_ms);
    ++watchdog_polls;
    if (pulse_after_battery && battery_polls != 0U)
        pulse_active = true;
}
} // extern "C"

bool app_param_store_load(uint8_t& offset) {
    const bool ready = next_stage(Params);
    offset = stored_offset; // 失败时故意保留非零值，验证 Runtime 主动回退 0。
    return ready && !load_fails;
}
bool app_battery_init(uint8_t offset) {
    loaded_offset = offset;
    return next_stage(Battery);
}
void app_battery_poll(uint32_t time) {
    check(!irq_masked && !unlocked && !pulse_active && time == now_ms);
    ++battery_polls;
    now_ms += battery_duration_ms;
    if (shutdown_after_battery)
        power.state = AppPowerState::ShutdownWait;
}
void app_battery_save_compensation(uint32_t time) {
    check(!irq_masked && !unlocked && !pulse_active && !response_pending && time == now_ms);
    ++save_polls;
    now_ms += save_duration_ms;
}
AppBatterySnapshot app_battery_snapshot(uint32_t time) {
    check(time == now_ms);
    return battery;
}
bool app_panel_init() {
    return next_stage(Panel);
}
void app_panel_poll(uint32_t time) {
    check(!irq_masked && !unlocked && time == now_ms);
}
bool app_panel_linux_seen() {
    return linux_seen;
}
bool app_panel_response_pending() {
    return response_pending;
}
bool app_keys_init(uint32_t time) {
    check(time == now_ms);
    return next_stage(Keys);
}
void app_keys_poll(uint32_t time) {
    check(!irq_masked && !unlocked && time == now_ms);
    ++key_polls;
}
void app_power_init(bool cold, uint32_t time) {
    check(cold == (reset_flags == RMU_FLAG_PWR_ON));
    check(irq_masked && !unlocked && time == now_ms);
    power.state = cold ? AppPowerState::Starting : AppPowerState::Running;
}
void app_power_poll(uint32_t time) {
    check(!irq_masked && !unlocked && time == now_ms);
    ++power_polls;
}
AppPowerSnapshot app_power_snapshot() {
    return power;
}
AppRecordingSnapshot app_recording_snapshot() {
    return recording;
}

int main() {
    // 任一关键初始化失败都应短路、恢复写保护并保持中断关闭。
    const unsigned failures[] = {Power, Hold,      Clock,    Pll,     Xtal,  Timebase,
                                 Led,   Indicator, Watchdog, Battery, Panel, Keys};
    for (unsigned failure : failures) {
        reset(failure);
        check(!app_runtime_init());
        check(stage == failure && irq_masked && !unlocked && watchdog_polls == 0U);
    }
    const uint32_t flags_list[] = {RMU_FLAG_PWR_ON, 2U, 0x100U, RMU_FLAG_PWR_ON | 2U};
    for (uint32_t flags : flags_list) {
        reset_flags = flags;
        reset();
        check(app_runtime_init());
        check(loaded_offset == stored_offset);
    }
    reset(Params);
    check(app_runtime_init() && loaded_offset == 0U); // 参数读失败不能阻止开机。
    reset();
    load_fails = true;
    check(app_runtime_init() && loaded_offset == 0U);

    reset(0U, UINT32_MAX - 500U);
    check(app_runtime_init());
    check(stage == Keys && !irq_masked && !unlocked);
    const uint32_t times[] = {UINT32_MAX - 1U, 0U, 499U, 500U, 1500U};
    for (uint32_t time : times)
        poll(time);
    check(watchdog_polls == 10U && key_polls == 5U && power_polls == 10U);

    // D7：只有 Running/Off，响应已读且脉冲低时进入保存入口。
    const AppPowerState states[] = {
        AppPowerState::Starting,  AppPowerState::Running,    AppPowerState::ShutdownWait, AppPowerState::ShutdownDelay,
        AppPowerState::ResetWait, AppPowerState::ResetReady, AppPowerState::Off};
    for (auto state : states) {
        start_running();
        power.state = state;
        poll(0U);
        check(save_polls == ((state == AppPowerState::Running || state == AppPowerState::Off) ? 1U : 0U));
    }
    start_running();
    response_pending = true;
    poll(0U);
    check(save_polls == 0U);
    response_pending = false;
    pulse_active = true;
    poll(1U);
    check(save_polls == 0U && battery_polls == 1U);
    pulse_active = false;
    poll(2U);
    check(save_polls == 1U);
    start_running();
    pulse_after_battery = true;
    battery_duration_ms = 3U;
    poll(0U);
    check(pulse_active && save_polls == 0U && app_runtime_diagnostics.max_work_ms == 3U);
    start_running();
    shutdown_after_battery = true;
    poll(0U);
    check(save_polls == 0U);
    expect_leds(true, false, true); // 新关机状态当轮驱动三灯。
    start_running();
    save_duration_ms = 4U;
    poll(0U);
    check(app_runtime_diagnostics.max_work_ms == 4U); // Flash 工作量计入 HIL 观测。

    // 实际 LED 模块：100 ms 输出节拍，模式变化立即输出，录制由 Linux 已出现门控。
    start_running();
    poll(0U);
    expect_leds(true, false, false);
    recording.active = true;
    poll(1U);
    expect_leds(true, false, false);
    check(led_writes == 3U);
    linux_seen = true;
    poll(2U);
    expect_leds(true, false, true);
    check(led_writes == 6U);
    poll(101U);
    check(led_writes == 6U);
    poll(102U);
    check(led_writes == 9U);
    poll(502U);
    expect_leds(true, false, false);
    power.state = AppPowerState::ShutdownWait;
    poll(503U);
    expect_leds(true, false, true);

    start_running();
    battery.charge_state = 2U;
    battery.percent = 98U;
    poll(0U);
    expect_leds(false, true, false);
    poll(300U);
    expect_leds(false, false, false);
    battery.charge_state = 1U;
    poll(301U);
    expect_leds(false, true, false);
    poll(1301U);
    expect_leds(false, false, false);
    battery.charge_state = 3U;
    poll(1302U);
    expect_leds(false, true, false);
    poll(2302U);
    expect_leds(false, true, false); // 终止态常亮，不按慢充闪烁。
    battery.charger_valid = false;
    poll(2303U);
    expect_leds(true, false, false);
    battery.charger_valid = true;
    battery.charge_state = 2U;
    battery.percent = 100U;
    poll(2304U);
    expect_leds(false, true, false);
    poll(2604U);
    expect_leds(false, true, false); // 补偿后 100% 常亮。
    battery.charge_state = 0U;
    battery.sample.soc_percent = 9U;
    battery.percent = 14U;
    poll(2605U);
    expect_leds(true, false, false); // LED 用补偿值，电源策略仍在独立回归中用原始值。
    battery.valid = false;
    poll(2606U);
    expect_leds(true, false, false);
    poll(5606U);
    expect_leds(false, false, false); // 无效样本的 3000 ms 翻转。

    std::puts("x1_runtime_tests: init / stored offset / D7 gates / LED mapping / cadence / wrap PASS");
    return 0;
}
