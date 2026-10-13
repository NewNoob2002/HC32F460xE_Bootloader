#include <assert.h>
#include <cstdlib>
#include <stdio.h>
#include <string.h>
#include "app_battery.h"
#include "app_param_store.h"
#include "bsp_charger_gpio.h"
#include "bsp_i2c_master.h"
#include "mp2762a.h"

namespace {
bsp_i2c_probe_result_t probe_result;
uint8_t registers[64];
Bq40z50Sample gauge{2982U, 7800U, 50U};
bool gauge_fails, mp_read_fails, mp_write_fails;
bool gpio_enabled, gpio_typec, gpio_detect;
unsigned gpio_inits, mp_writes;
unsigned param_save_calls;
uint8_t mock_charge_state, saved_offset;
bool param_save_fails;
void check(bool ok) {
    if (!ok)
        std::abort();
}
void reset(bsp_i2c_probe_result_t probe, uint8_t offset = 0U) {
    probe_result = probe;
    memset(registers, 0, sizeof(registers));
    registers[0] = 0x1EU;
    registers[8] = 0x16U;
    registers[0x13] = 2U;
    gauge = {2982U, 7800U, 50U};
    gauge_fails = mp_read_fails = mp_write_fails = false;
    gpio_enabled = false;
    gpio_typec = gpio_detect = true;
    gpio_inits = mp_writes = param_save_calls = 0U;
    mock_charge_state = 2U;
    saved_offset = 0xFFU;
    param_save_fails = false;
    check(app_battery_init(offset));
}
} // namespace
extern "C" {
bool bsp_i2c_master_init(void) {
    return true;
}
bsp_i2c_probe_result_t bsp_i2c_master_probe(uint8_t address) {
    assert(address == kMp2762aDeviceAddress);
    return probe_result;
}
bool bsp_i2c_master_read(uint8_t address, uint8_t reg, uint8_t* data, uint8_t size) {
    assert(address == kMp2762aDeviceAddress && reg < sizeof(registers) && size == 1U);
    if (mp_read_fails)
        return false;
    *data = registers[reg];
    return true;
}
bool bsp_i2c_master_write_byte(uint8_t address, uint8_t reg, uint8_t value) {
    assert(address == kMp2762aDeviceAddress && reg < sizeof(registers));
    ++mp_writes;
    if (mp_write_fails)
        return false;
    if (reg == 8U) {
        assert((value & 0xC1U) == 0U); // 不误触发REG_RST/WTD_RST/保留位。
        registers[0x13] =
            (value & 0x10U) ? static_cast<uint8_t>(2U | (static_cast<uint32_t>(mock_charge_state) << 2U)) : 2U;
    }
    registers[reg] = value;
    return true;
}
bool bsp_i2c_master_read_word(uint8_t address, uint8_t reg, uint16_t* value) {
    assert(address == kBq40z50DeviceAddress);
    if (gauge_fails)
        return false;
    if (reg == kBq40z50Temperature)
        *value = gauge.temperature_dk;
    else if (reg == kBq40z50Voltage)
        *value = gauge.voltage_mv;
    else {
        assert(reg == kBq40z50RelativeStateOfCharge);
        *value = gauge.soc_percent;
    }
    return true;
}
bool bsp_charger_gpio_init(void) {
    ++gpio_inits;
    gpio_enabled = false;
    return true;
}
bool bsp_charger_gpio_typec_present(void) {
    return gpio_typec;
}
bool bsp_charger_gpio_detect(void) {
    return gpio_detect;
}
void bsp_charger_gpio_enable(bool enabled) {
    assert(gpio_inits > 0U);
    gpio_enabled = enabled;
}
}
bool app_param_store_save(uint8_t offset) {
    ++param_save_calls;
    saved_offset = offset;
    return !param_save_fails;
}

int main() {
    reset(BSP_I2C_PROBE_IO_ERROR);
    const uint32_t t4[] = {0U, 200U, 400U, 600U};
    for (uint32_t time : t4)
        app_battery_poll(time);
    assert(app_battery_snapshot(600U).hardware == AppHardware::Unknown && gpio_inits == 0U && mp_writes == 0U);
    reset(BSP_I2C_PROBE_ADDRESS_NACK);
    gauge_fails = true;
    const uint32_t t3[] = {0U, 200U, 400U};
    for (uint32_t time : t3)
        app_battery_poll(time);
    assert(app_battery_snapshot(400U).hardware == AppHardware::Unknown && gpio_inits == 0U);
    reset(BSP_I2C_PROBE_ADDRESS_NACK);
    app_battery_poll(0U);
    app_battery_poll(200U);
    assert(gpio_inits == 0U);
    app_battery_poll(400U);
    assert(app_battery_snapshot(400U).hardware == AppHardware::GpioV13 && gpio_inits == 1U && !gpio_enabled);
    app_battery_poll(1900U);
    assert(gpio_enabled && app_battery_snapshot(1900U).charge_state == 2U);
    probe_result = BSP_I2C_PROBE_PRESENT;
    gauge.temperature_dk = 3182U;
    app_battery_poll(3400U);
    assert(!gpio_enabled && mp_writes == 0U); // 高温停充且识别后不切换方案。
    gauge.soc_percent = 0U;
    gauge.temperature_dk = 2731U;
    app_battery_poll(6400U);
    assert(app_battery_snapshot(6400U).valid && !gpio_enabled); // 0%有效，0°C以下不充电。
    gauge.temperature_dk = 3000U;
    app_battery_poll(9400U);
    assert(gpio_enabled);
    gauge_fails = true;
    app_battery_poll(12400U);
    assert(!app_battery_snapshot(12400U).valid && !gpio_enabled);
    reset(BSP_I2C_PROBE_PRESENT);
    app_battery_poll(0U);
    auto state = app_battery_snapshot(0U);
    assert(state.hardware == AppHardware::Mp2762V15Plus && gpio_inits == 0U && state.charger_valid);
    assert(state.charging_enabled && state.charge_state == 2U);
    assert((registers[7] & 0x30U) == 0U && (registers[0x30] & 8U) == 8U);
    assert((registers[3] & 0xF0U) == 0xF0U && (registers[2] & 0x7FU) == 32U);
    mp_read_fails = true;
    probe_result = BSP_I2C_PROBE_ADDRESS_NACK;
    app_battery_poll(500U);
    state = app_battery_snapshot(500U);
    assert(state.hardware == AppHardware::Mp2762V15Plus && !state.charger_valid && gpio_inits == 0U);
    reset(BSP_I2C_PROBE_PRESENT);
    registers[0x13] |= 0x20U;
    for (uint32_t time : t3)
        app_battery_poll(time);
    assert(app_battery_snapshot(400U).hardware == AppHardware::Unknown && gpio_inits == 0U);
    reset(BSP_I2C_PROBE_PRESENT);
    mp_write_fails = true;
    app_battery_poll(0U);
    assert(!app_battery_snapshot(0U).charger_valid && gpio_inits == 0U);
    reset(BSP_I2C_PROBE_PRESENT);
    app_battery_poll(UINT32_MAX - 100U);
    assert(app_battery_snapshot(99U).valid);
    assert(!app_battery_snapshot(5900U).valid); // 陈旧判断跨回绕。
    gauge.soc_percent = 101U;
    Bq40z50Sample unchanged{1U, 2U, 3U};
    assert(!bq40z50_read_sample(unchanged) && unchanged.soc_percent == 3U);
    // 已保存偏移直接用于快照；没有新计算结果时不触碰 Flash。
    reset(BSP_I2C_PROBE_PRESENT, 5U);
    gauge.soc_percent = 9U;
    app_battery_poll(0U);
    check(app_battery_snapshot(0U).percent == 14U && app_battery_snapshot(0U).sample.soc_percent == 9U);
    app_battery_save_compensation(0U);
    check(param_save_calls == 0U);

    // 真实电池业务经过 20 min：预充/快充/终止都生成 offset=2，失败重试跨回绕且成功后只保存一次。
    constexpr uint32_t window_ms = 20U * 60U * 1000U;
    const uint8_t charge_states[] = {1U, 2U, 3U};
    for (uint8_t charge_state : charge_states) {
        reset(BSP_I2C_PROBE_PRESENT, 5U);
        mock_charge_state = charge_state;
        gauge.soc_percent = 98U;
        const uint32_t start = UINT32_MAX - window_ms - 1000U;
        for (uint32_t elapsed = 0U; elapsed <= window_ms; elapsed += 500U)
            app_battery_poll(start + elapsed);
        const uint32_t save_ms = start + window_ms;
        check(app_battery_snapshot(save_ms).charge_state == charge_state);
        check(app_battery_snapshot(save_ms).percent == 100U);
        check(param_save_calls == 0U);
        param_save_fails = true;
        app_battery_save_compensation(save_ms);
        check(param_save_calls == 1U && saved_offset == 2U);
        param_save_fails = false;
        app_battery_save_compensation(save_ms + 2999U);
        check(param_save_calls == 1U);
        app_battery_save_compensation(save_ms + 3000U);
        check(param_save_calls == 2U && saved_offset == 2U);
        app_battery_save_compensation(save_ms + 6000U);
        check(param_save_calls == 2U);
        check(app_battery_init(2U));
        app_battery_poll(0U);
        app_battery_save_compensation(0U);
        check(param_save_calls == 2U && app_battery_snapshot(0U).percent == 100U);
    }
    puts("x1_battery_tests: hardware / temperature / data failures / stored SOC / save retry PASS");
}
