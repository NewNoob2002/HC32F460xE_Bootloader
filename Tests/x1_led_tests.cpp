#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include "app_led.h"

namespace {
// 不用 assert：app_led_level() 有副作用，NDEBUG 下不能被整段删除。
void check(bool ok) {
    if (!ok)
        std::abort();
}
AppLedInput base() {
    return {AppPowerState::Running, true, false, true, false, false, false, 50U};
}
bool same(AppLedPattern pattern, AppLedMode power, AppLedMode charge, AppLedMode function) {
    return pattern.power == power && pattern.charge == charge && pattern.function == function;
}
} // namespace

int main() {
    using M = AppLedMode;

    // §5 关机流程最高优先级，即使正在充电或录制。
    auto in = base();
    in.charging = true;
    in.recording = true;
    in.percent = 100U;
    for (auto state : {AppPowerState::ShutdownWait, AppPowerState::ShutdownDelay}) {
        in.power = state;
        check(same(app_led_select(in), M::On, M::Off, M::On));
    }

    // Off / Starting 驻留：电源和功能灯灭，充电灯仍按充电规则。
    in = base();
    in.power = AppPowerState::Starting;
    check(same(app_led_select(in), M::Off, M::Off, M::Off));
    in.charging = true;
    in.fast_charging = true;
    check(same(app_led_select(in), M::Off, M::Toggle300Ms, M::Off));
    in.power = AppPowerState::Off;
    in.fast_charging = false;
    check(same(app_led_select(in), M::Off, M::Toggle1000Ms, M::Off));

    // 充电中：补偿后 SOC≥100 常亮（有效样本才判定），快充 300 ms，其他 1000 ms。
    in = base();
    in.charging = true;
    in.percent = 99U;
    in.fast_charging = true;
    check(same(app_led_select(in), M::Off, M::Toggle300Ms, M::Off));
    in.fast_charging = false;
    check(same(app_led_select(in), M::Off, M::Toggle1000Ms, M::Off));
    in.percent = 100U;
    check(same(app_led_select(in), M::Off, M::On, M::Off));
    in.battery_valid = false;
    check(same(app_led_select(in), M::Off, M::Toggle1000Ms, M::Off));
    in.battery_valid = true;
    in.fast_charging = true;
    check(same(app_led_select(in), M::Off, M::On, M::Off));

    // 充电器终止（charge_state=3）：充电灯常亮，不随补偿后 SOC 变化而闪烁，电源灯灭。
    in = base();
    in.charge_done = true;
    in.percent = 98U;
    check(same(app_led_select(in), M::Off, M::On, M::Off));
    in.percent = 100U;
    check(same(app_led_select(in), M::Off, M::On, M::Off));
    in.power = AppPowerState::Starting;
    check(same(app_led_select(in), M::Off, M::On, M::Off));
    in.power = AppPowerState::ShutdownWait;
    check(same(app_led_select(in), M::On, M::Off, M::On));

    // 未充电：≥14 常亮，10–13 1000 ms，0–9 300 ms（含 0%），样本无效 3000 ms。
    in = base();
    in.percent = 14U;
    check(same(app_led_select(in), M::On, M::Off, M::Off));
    in.percent = 13U;
    check(same(app_led_select(in), M::Toggle1000Ms, M::Off, M::Off));
    in.percent = 10U;
    check(same(app_led_select(in), M::Toggle1000Ms, M::Off, M::Off));
    in.percent = 9U;
    check(same(app_led_select(in), M::Toggle300Ms, M::Off, M::Off));
    in.percent = 0U;
    check(same(app_led_select(in), M::Toggle300Ms, M::Off, M::Off));
    in.percent = 100U;
    in.battery_valid = false;
    check(same(app_led_select(in), M::Toggle3000Ms, M::Off, M::Off));

    // 录制：500 ms 翻转；需要 Linux 已有有效通信，且仅在录制中。
    in = base();
    in.recording = true;
    in.linux_seen = false;
    check(app_led_select(in).function == M::Off);
    in.linux_seen = true;
    check(app_led_select(in).function == M::Toggle500Ms);
    in.recording = false;
    check(app_led_select(in).function == M::Off);
    in.recording = true;
    in.charging = true;
    check(app_led_select(in).function == M::Toggle500Ms);
    in.power = AppPowerState::Off;
    check(app_led_select(in).function == M::Off);

    // 复位等待按运行中规则显示。
    in = base();
    in.power = AppPowerState::ResetWait;
    in.percent = 14U;
    check(same(app_led_select(in), M::On, M::Off, M::Off));

    // 时序：常亮/熄灭；翻转从亮开始，间隔边界精确。
    AppLedChannel ch{M::Off, 0U};
    check(!app_led_level(ch, M::Off, 0U));
    check(app_led_level(ch, M::On, 10U));
    check(app_led_level(ch, M::On, 999999U));
    ch = {M::Off, 0U};
    check(app_led_level(ch, M::Toggle300Ms, 1000U));
    check(app_led_level(ch, M::Toggle300Ms, 1299U));
    check(!app_led_level(ch, M::Toggle300Ms, 1300U));
    check(!app_led_level(ch, M::Toggle300Ms, 1599U));
    check(app_led_level(ch, M::Toggle300Ms, 1600U));

    // 模式切换立即从新模式的亮相位开始，不继承旧相位。
    check(app_led_level(ch, M::Toggle1000Ms, 1700U));
    check(app_led_level(ch, M::Toggle1000Ms, 2699U));
    check(!app_led_level(ch, M::Toggle1000Ms, 2700U));

    // 32 位毫秒回绕后相位保持：起点附近 600 ms 应重新亮起。
    ch = {M::Off, 0U};
    const uint32_t start = UINT32_MAX - 100U;
    check(app_led_level(ch, M::Toggle300Ms, start));
    check(!app_led_level(ch, M::Toggle300Ms, start + 300U));
    check(app_led_level(ch, M::Toggle300Ms, start + 600U));
    check(app_led_level(ch, M::Toggle300Ms, start + 600U + 299U));

    // 连续运行超过 2^32 ms（约 49.7 天）后相位仍正确：每次推进 1e9 ms，单次差值始终可表示。
    ch = {M::Off, 0U};
    check(app_led_level(ch, M::Toggle300Ms, 0U));
    uint64_t true_ms = 0U;
    for (unsigned step = 0U; step < 6U; ++step) {
        true_ms += 1000000000ULL;
        const bool expected = ((true_ms / 300U) & 1U) == 0U;
        check(app_led_level(ch, M::Toggle300Ms, static_cast<uint32_t>(true_ms)) == expected);
    }

    std::puts("x1_led_tests: §5 rows / priority / SOC boundaries / recording gate / toggle timing / wrap PASS");
    return 0;
}
