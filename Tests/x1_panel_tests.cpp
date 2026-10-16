#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app_battery.h"
#include "app_panel.h"
#include "app_power.h"
#include "app_recording.h"
#include "bsp_i2c_slave.h"
#include "panel_protocol.h"

namespace {
uint8_t input[256], output[256];
size_t input_size, output_size;
bool reserved, busy, publish_fails, recover;
uint32_t completed;
unsigned resets, releases;
// 原始 95%、补偿后 100%：0x01 必须上报补偿值（D3）。
AppBatterySnapshot battery{AppHardware::GpioV13, {3000U, 7800U, 95U}, 0U, true, true, false, false, 0U, 100U};

void request(uint16_t id, uint8_t type, const uint8_t* payload, size_t size, uint32_t time) {
    input_size = panel_encode_response(id, type, payload, size, input, sizeof(input));
    assert(input_size);
    app_panel_poll(time);
    assert(app_panel_response_pending() == busy);
}
void finish(uint32_t time = 0U) {
    busy = false;
    ++completed;
    app_panel_poll(time);
    assert(!app_panel_response_pending());
}
} // namespace

extern "C" {
void bsp_reset_request(void) {
    ++resets;
}
void bsp_power_hold_deassert(void) {
    ++releases;
}
bool bsp_i2c_slave_init(void) {
    reserved = busy = false;
    completed = 0U;
    input_size = output_size = 0U;
    return true;
}
void bsp_i2c_slave_get_counters(bsp_i2c_slave_counters_t* counters) {
    *counters = {};
    counters->tx_complete_reads = completed;
}
size_t bsp_i2c_slave_read(uint8_t* bytes, size_t capacity) {
    assert(input_size <= capacity);
    const size_t size = input_size;
    memcpy(bytes, input, size);
    input_size = 0U;
    return size;
}
bool bsp_i2c_slave_poll(uint32_t) {
    const bool result = recover;
    recover = false;
    return result;
}
bool bsp_i2c_slave_tx_reserve(void) {
    if (busy || reserved)
        return false;
    reserved = true;
    return true;
}
void bsp_i2c_slave_tx_cancel_write(void) {
    reserved = false;
}
int bsp_i2c_slave_tx_write(const uint8_t* bytes, uint16_t size) {
    assert(reserved);
    reserved = false;
    if (publish_fails)
        return 0;
    assert(!busy);
    memcpy(output, bytes, size);
    output_size = size;
    busy = true;
    return size;
}
}
AppBatterySnapshot app_battery_snapshot(uint32_t) {
    return battery;
}
const char* app_hardware_name(AppHardware) {
    return "V1.3";
}
[[noreturn]] void app_runtime_fault() {
    assert(false);
    for (;;) {}
}

int main() {
    // 独立从真实Linux采集的CRC32黄金帧，不用编码器生成请求来验证编码器本身。
    const uint8_t linux_query[] = {0xAA, 0x44, 0x18, 0x14, 0x02, 0, 0, 0, 0, 0,    0,    0,    2,
                                   0,    0,    0,    0,    0,    0, 0, 1, 0, 0xA6, 0x05, 0x5D, 0x81};
    assert(panel_crc32(linux_query, 22U) == 0x815D05A6U);
    assert(panel_crc32(reinterpret_cast<const uint8_t*>("123456789"), 9U) == 0xCBF43926U);
    PanelParser parser{};
    unsigned callbacks = 0U;
    const auto callback = [](const PanelFrame& frame, void* context) {
        assert(frame.id == 2U && frame.type == 0U && frame.payload_size == 2U && frame.payload[0] == 1U);
        ++*static_cast<unsigned*>(context);
    };
    for (uint8_t byte : linux_query)
        panel_parser_push(parser, &byte, 1U, 0U, callback, &callbacks);
    assert(callbacks == 1U && parser.valid_frames == 1U && parser.size == 0U);
    uint8_t corrupted[sizeof(linux_query)];
    memcpy(corrupted, linux_query, sizeof(corrupted));
    corrupted[20] ^= 1U;
    panel_parser_push(parser, corrupted, sizeof(corrupted), 1U, callback, &callbacks);
    assert(parser.crc_errors == 1U && callbacks == 1U);
    memcpy(corrupted, linux_query, sizeof(corrupted));
    corrupted[12] = corrupted[13] = 0xFFU;
    panel_parser_push(parser, corrupted, 20U, 2U, callback, &callbacks);
    assert(parser.length_errors == 1U && parser.size == 0U);
    panel_parser_push(parser, linux_query, 5U, UINT32_MAX - 10U, callback, &callbacks);
    panel_parser_poll(parser, 89U);
    assert(parser.timeouts == 1U && parser.size == 0U);
    panel_parser_push(parser, nullptr, 100U, 90U, callback, &callbacks);
    assert(callbacks == 1U);

    assert(app_panel_init());
    assert(!app_panel_linux_seen() && !app_panel_response_pending());
    app_power_init(false, 0U);
    memcpy(input, linux_query, sizeof(linux_query));
    input[sizeof(linux_query) - 1U] ^= 1U;
    input_size = sizeof(linux_query);
    app_panel_poll(0U);
    assert(!app_panel_linux_seen() && !app_panel_response_pending());
    memcpy(input, linux_query, sizeof(linux_query));
    input_size = sizeof(linux_query);
    app_panel_poll(0U);
    assert(output_size == 32U && busy && output[17] == 1U);
    assert(app_panel_linux_seen() && app_panel_response_pending());
    app_panel_poll(0U);
    assert(app_panel_response_pending()); // 无完整读取计数时保留未读状态。
    for (size_t i = 20U; i < 28U; ++i)
        assert(output[i] == 0U);
    finish();
    const uint8_t zero[] = {0U, 0U};
    request(1U, 0U, zero, sizeof(zero), 0U);
    assert(output_size == 60U && output[16] == 3U && output[17] == 1U);
    assert(memcmp(output + 20U, "V1.3", 4U) == 0);
    assert(memcmp(output + 28U, "X1-P5", 5U) == 0);
    assert(output[38] == 100U && output[39] == 0U && output[40] == 0xB8U && output[41] == 0x0BU);
    assert(output[42] == 7U && output[46] == 1U);
    assert(output[43] == 0U && output[44] == 0U && output[45] == 0U); // 无旧响应残留。
    finish();
    uint8_t sync[8]{};
    sync[0] = 1U;
    publish_fails = true;
    request(3U, 2U, sync, sizeof(sync), 0U);
    assert(!app_recording_snapshot().active && !busy && !reserved);
    publish_fails = false;
    request(3U, 2U, sync, sizeof(sync), 0U);
    assert(output_size == 26U && output[20] == 1U && app_recording_snapshot().active);
    sync[0] = 0U;
    request(3U, 2U, sync, sizeof(sync), 0U);
    assert(app_recording_snapshot().active); // 未读响应不能被下一命令覆盖。
    finish();
    app_recording_toggle();
    request(2U, 0U, zero, sizeof(zero), 0U);
    assert(output[23] == 1U);
    app_recording_toggle(); // 响应发布后发生新操作。
    recover = true;
    app_panel_poll(0U); // 恢复不误清除未读操作。
    assert(app_recording_snapshot().operation_pending && app_panel_response_pending());
    finish();
    assert(app_recording_snapshot().operation_pending);
    request(2U, 0U, zero, sizeof(zero), 0U);
    finish();
    assert(!app_recording_snapshot().operation_pending);
    request(4U, 2U, sync, sizeof(sync), 0U);
    assert(output_size == 26U && output[20] == 0U); // 0参数不执行复位。
    finish();
    request(6U, 2U, sync, sizeof(sync), 0U);
    assert(!busy && !reserved);
    request(3U, 2U, nullptr, 0U, 0U);
    assert(!busy);
    battery.valid = false;
    request(1U, 0U, zero, sizeof(zero), 0U);
    assert(!busy && !reserved); // 不能伪造电池采样。

    // 充电数据失效时，关机通知与关联停录制操作仍能发送；读通知不等于关机完成。
    app_power_key_long_press(100U);
    battery.charger_valid = false;
    request(2U, 0U, zero, sizeof(zero), 101U);
    assert(output_size == 32U && output[21] == 1U && output[24] == 0U);
    finish(102U);
    app_power_poll(8102U);
    assert(releases == 0U && app_power_snapshot().state == AppPowerState::ShutdownWait);
    sync[0] = 1U;
    publish_fails = true;
    request(5U, 2U, sync, sizeof(sync), 8103U);
    assert(app_power_snapshot().state == AppPowerState::ShutdownWait && !busy);
    publish_fails = false;
    request(5U, 2U, sync, sizeof(sync), 8104U);
    assert(output[20] == 1U && app_power_snapshot().state == AppPowerState::ShutdownDelay);
    finish(8105U);
    request(5U, 2U, sync, sizeof(sync), 9000U);
    finish(9001U);
    app_power_poll(16103U);
    assert(releases == 0U);
    app_power_poll(16104U);
    assert(releases == 1U);
    request(4U, 2U, sync, sizeof(sync), 16105U);
    assert(output[20] == 0U);
    finish(16106U);

    // 复位副作用必须在成功发布后且对应 ACK 完整读出才执行。
    assert(app_panel_init());
    app_power_init(false, 0U);
    publish_fails = true;
    request(4U, 2U, sync, sizeof(sync), 100U);
    assert(app_power_snapshot().state == AppPowerState::Running && resets == 0U);
    publish_fails = false;
    request(4U, 2U, sync, sizeof(sync), 100U);
    assert(output[20] == 1U && app_power_snapshot().state == AppPowerState::ResetWait);
    app_panel_poll(200U); // 短读/尚未读取不能推进完成计数。
    request(5U, 2U, sync, sizeof(sync), 300U);
    assert(app_power_snapshot().state == AppPowerState::ResetWait && resets == 0U);
    finish(400U);
    assert(app_power_snapshot().state == AppPowerState::ResetReady && resets == 0U);
    request(3U, 2U, sync, sizeof(sync), 401U);
    assert(!busy); // ACK完成到主循环复位之间也禁止新副作用。
    app_power_poll(401U);
    assert(resets == 1U);

    assert(app_panel_init());
    app_power_init(false, 0U);
    request(4U, 2U, sync, sizeof(sync), 100U);
    request(4U, 2U, sync, sizeof(sync), 1000U);
    app_power_poll(5099U);
    assert(app_power_snapshot().state == AppPowerState::ResetWait);
    app_power_poll(5100U);
    assert(app_power_snapshot().state == AppPowerState::Running && busy);
    request(2U, 0U, zero, sizeof(zero), 5200U);
    assert(busy && output[4] == 4U); // 超时取消仍保留原ACK，不覆盖它。
    finish(6000U);
    app_power_poll(6000U);
    assert(resets == 1U);

    // 低电量停录制不能被主机同步重新开启。
    battery.valid = battery.charger_valid = true;
    battery.sample.soc_percent = 9U;
    app_power_poll(6001U);
    request(3U, 2U, sync, sizeof(sync), 6002U);
    assert(output[20] == 0U && !app_recording_snapshot().active);
    finish(6003U);
    assert(app_panel_init());
    assert(!app_panel_linux_seen());
    request(6U, 2U, sync, sizeof(sync), 6004U);
    assert(app_panel_linux_seen() && !app_panel_response_pending()); // 未支持消息仍证明有效通信。
    puts("x1_panel_tests: real frame / bounds / busy / operation ownership / shutdown / reset ACK PASS");
}
