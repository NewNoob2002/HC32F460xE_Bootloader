#include "app_battery_compensation.h"
#include "bsp_timebase.h"

namespace {
constexpr uint32_t stable_window_ms = 20U * 60U * 1000U;
constexpr uint8_t max_offset = 9U;
} // namespace

void app_soc_compensation_init(AppSocCompensation& comp, uint8_t stored_offset) {
    comp = {};
    comp.offset = stored_offset <= max_offset ? stored_offset : 0U;
}

void app_soc_compensation_update(AppSocCompensation& comp, bool sample_valid, uint16_t raw_soc, bool charging,
                                 uint32_t now_ms) {
    if (!charging) {
        comp.timing = false;
        comp.session_done = false; // D4：下次充电可重新计算。
        return;
    }
    if (!sample_valid || comp.session_done) {
        comp.timing = false;
        return;
    }
    if (!comp.timing || (raw_soc != comp.stable_soc)) {
        comp.timing = true;
        comp.stable_soc = raw_soc;
        comp.stable_since_ms = now_ms;
        return;
    }
    if (!bsp_time_elapsed(now_ms, comp.stable_since_ms, stable_window_ms))
        return;
    comp.timing = false;
    comp.session_done = true;
    const uint16_t offset = raw_soc >= 100U ? 0U : static_cast<uint16_t>(100U - raw_soc);
    if (offset > max_offset) {
        // D2：旧代码不保存但仍在 RAM 生效；这里整体丢弃，保持原偏移。
        ++comp.rejected;
        return;
    }
    if (offset != comp.offset) {
        comp.offset = static_cast<uint8_t>(offset);
        comp.save_pending = true;
    }
}

uint8_t app_soc_compensation_percent(const AppSocCompensation& comp, uint16_t raw_soc) {
    const uint32_t percent = static_cast<uint32_t>(raw_soc) + comp.offset;
    return static_cast<uint8_t>(percent > 100U ? 100U : percent);
}
