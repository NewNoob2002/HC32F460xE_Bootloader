#include <cstdio>
#include <cstdlib>
#include "app_battery_compensation.h"

namespace {
constexpr uint32_t WINDOW = 20U * 60U * 1000U;
// 不用 assert：被测调用有副作用，NDEBUG 下不能被整段删除。
void check(bool ok) {
    if (!ok)
        std::abort();
}
void run(AppSocCompensation& comp, uint16_t soc, uint32_t from, uint32_t to, bool charging = true) {
    for (uint32_t t = from;; t += 500U) {
        app_soc_compensation_update(comp, true, soc, charging, t);
        if (to - t < 500U)
            break;
    }
}
} // namespace

int main() {
    AppSocCompensation comp{};

    // 存储值越界视为损坏。
    app_soc_compensation_init(comp, 10U);
    check(comp.offset == 0U);
    app_soc_compensation_init(comp, 9U);
    check(comp.offset == 9U && app_soc_compensation_percent(comp, 95U) == 100U);
    check(app_soc_compensation_percent(comp, 0U) == 9U);

    // 20 min 差 1 ms 不触发，刚好触发；终止态 3 也算充电（由调用方映射为 charging）。
    app_soc_compensation_init(comp, 0U);
    app_soc_compensation_update(comp, true, 98U, true, 1000U);
    app_soc_compensation_update(comp, true, 98U, true, 1000U + WINDOW - 1U);
    check(comp.offset == 0U && !comp.save_pending);
    app_soc_compensation_update(comp, true, 98U, true, 1000U + WINDOW);
    check(comp.offset == 2U && comp.save_pending && app_soc_compensation_percent(comp, 98U) == 100U);

    // 同一会话只算一次。
    comp.save_pending = false;
    run(comp, 97U, 1000U + WINDOW, 1000U + 3U * WINDOW);
    check(comp.offset == 2U && !comp.save_pending);

    // 拔出后新会话可覆盖；值不变不请求保存。
    app_soc_compensation_update(comp, true, 98U, false, 0U);
    run(comp, 98U, 0U, WINDOW);
    check(comp.offset == 2U && !comp.save_pending && comp.session_done);

    // 中途 SOC 变化重新计时。
    app_soc_compensation_init(comp, 0U);
    run(comp, 97U, 0U, WINDOW - 500U);
    run(comp, 99U, WINDOW, 2U * WINDOW - 500U);
    check(comp.offset == 0U);
    app_soc_compensation_update(comp, true, 99U, true, 2U * WINDOW);
    check(comp.offset == 1U && comp.save_pending);

    // 无效/陈旧样本、拔出均清零计时。
    app_soc_compensation_init(comp, 0U);
    run(comp, 96U, 0U, WINDOW - 500U);
    app_soc_compensation_update(comp, false, 96U, true, WINDOW - 250U);
    app_soc_compensation_update(comp, true, 96U, true, WINDOW);
    check(!comp.session_done);
    run(comp, 96U, WINDOW, 2U * WINDOW - 500U);
    app_soc_compensation_update(comp, true, 96U, false, 2U * WINDOW - 250U);
    app_soc_compensation_update(comp, true, 96U, true, 2U * WINDOW);
    check(!comp.session_done && comp.offset == 0U);

    // offset≥10 丢弃并计数，保留原偏移，本会话不再计算。
    app_soc_compensation_init(comp, 3U);
    run(comp, 90U, 0U, WINDOW);
    check(comp.offset == 3U && comp.rejected == 1U && !comp.save_pending && comp.session_done);

    // 原始 100% 稳定 → offset 0，覆盖旧偏移。
    app_soc_compensation_init(comp, 4U);
    run(comp, 100U, 0U, WINDOW);
    check(comp.offset == 0U && comp.save_pending);
    check(app_soc_compensation_percent(comp, 100U) == 100U);

    // 跨 2^32 ms 回绕。
    app_soc_compensation_init(comp, 0U);
    const uint32_t start = UINT32_MAX - 1000U;
    app_soc_compensation_update(comp, true, 95U, true, start);
    app_soc_compensation_update(comp, true, 95U, true, start + WINDOW - 1U);
    check(comp.offset == 0U);
    app_soc_compensation_update(comp, true, 95U, true, start + WINDOW);
    check(comp.offset == 5U && comp.save_pending);

    puts("x1_battery_compensation_tests: window / session / reset / reject / wrap PASS");
    return 0;
}
