#define LOG_TAG "Panel"
#include "app_panel.h"
#include <string.h>
#include "app_battery.h"
#include "app_power.h"
#include "app_recording.h"
#include "app_runtime.h"
#include "bsp_i2c_slave.h"
#include "firmware_log.h"
#include "panel_protocol.h"

namespace {
PanelParser parser{};
uint8_t rx_buffer[BSP_I2C_RX_CAPACITY];
uint8_t response[60U];
uint32_t now_ms;
uint32_t completion_count;
uint32_t confirmed_responses;
uint32_t published_responses;
uint32_t rejected_requests;
uint32_t unsupported_requests;
uint32_t busy_requests;
uint32_t recovery_failures;
bool response_pending;
bool response_has_operation;
bool response_has_reset;
uint32_t response_operation;

void put_word(uint8_t* bytes, uint16_t value) {
    bytes[0] = static_cast<uint8_t>(value);
    bytes[1] = static_cast<uint8_t>(value >> 8U);
}

void handle_frame(const PanelFrame& frame, void*) {
    // 对已支持消息验证实际消费的字段、消息类型和长度；未知消息不执行副作用。
    const bool query = (frame.id == 1U) || (frame.id == 2U);
    const bool set = (frame.id >= 3U) && (frame.id <= 5U);
    if (!query && !set) {
        ++unsupported_requests;
        return;
    }
    if ((query && ((frame.type != 0U) || (frame.payload_size < 2U)))
        || (set && ((frame.type != 2U) || (frame.payload_size < 1U))) || (set && (frame.payload[0] > 1U))) {
        ++rejected_requests;
        return;
    }
    if (response_pending || (app_power_snapshot().state == AppPowerState::ResetReady) || !bsp_i2c_slave_tx_reserve()) {
        ++busy_requests;
        return;
    }
    uint8_t payload[36U]{};
    size_t payload_size = 2U;
    bool includes_operation = false;
    const auto recording = app_recording_snapshot();
    const auto power = app_power_snapshot();
    const auto battery = app_battery_snapshot(now_ms);
    if ((frame.id == 1U) && battery.valid && (battery.hardware != AppHardware::Unknown)) {
        payload_size = 36U;
        const char* hardware = app_hardware_name(battery.hardware);
        memcpy(payload, hardware, strlen(hardware) + 1U);
        constexpr char version[] = "X1-P3";
        memcpy(payload + 8U, version, sizeof(version) - 1U);
        put_word(payload + 18U, battery.sample.soc_percent);
        put_word(payload + 20U, battery.sample.temperature_dk);
        // 保持旧面板线缆的整数伏特字段，内部采样仍保留mV。
        put_word(payload + 22U, static_cast<uint16_t>(battery.sample.voltage_mv / 1000U));
        payload[26U] = 1U;
    } else if (frame.id == 2U) {
        payload_size = 8U;
        payload[0U] = (power.state == AppPowerState::ResetWait) ? 1U : 0U;
        payload[1U] = power.shutdown_requested ? 1U : 0U;
        payload[2U] = recording.active ? 1U : 0U;
        payload[3U] = recording.operation_pending ? 1U : 0U;
        // 此字段只表示已知正在充电；充电数据失败不能阻止关机通知。
        payload[4U] = battery.charger_valid && (battery.charge_state == 1U || battery.charge_state == 2U) ? 1U : 0U;
        includes_operation = recording.operation_pending;
    } else if (frame.id == 3U) {
        payload[0U] = (frame.payload[0U] == 0U || recording.allowed) ? 1U : 0U;
    } else if (frame.id == 4U) {
        payload[0U] = (frame.payload[0U] != 0U && app_power_can_reset()) ? 1U : 0U;
    } else if (frame.id == 5U) {
        payload[0U] = (frame.payload[0U] != 0U && app_power_can_confirm_shutdown()) ? 1U : 0U;
    } else {
        ++rejected_requests;
        bsp_i2c_slave_tx_cancel_write();
        return; // 无有效电池/充电状态时不伪造正常查询响应。
    }
    const size_t size =
        panel_encode_response(frame.id, query ? 1U : 3U, payload, payload_size, response, sizeof(response));
    if ((size == 0U) || (bsp_i2c_slave_tx_write(response, static_cast<uint16_t>(size)) != static_cast<int>(size))) {
        bsp_i2c_slave_tx_cancel_write();
        return;
    }
    response_pending = true;
    response_has_operation = includes_operation;
    response_has_reset = (frame.id == 4U) && (payload[0U] == 1U);
    response_operation = recording.operation_sequence;
    ++published_responses;
    if (payload[0U] == 1U) {
        if (frame.id == 3U)
            app_recording_sync(frame.payload[0U] != 0U);
        else if (frame.id == 4U)
            app_power_request_reset(now_ms);
        else if (frame.id == 5U)
            app_power_confirm_shutdown(now_ms);
    }
    if (published_responses <= 8U)
        FIRMWARE_LOG_INFO("reply id=%u len=%u", static_cast<unsigned>(frame.id), static_cast<unsigned>(size));
}
} // namespace

bool app_panel_init() {
    parser = {};
    app_recording_init();
    response_pending = false;
    response_has_operation = false;
    response_has_reset = false;
    published_responses = 0U;
    confirmed_responses = 0U;
    rejected_requests = 0U;
    unsupported_requests = 0U;
    busy_requests = 0U;
    if (!bsp_i2c_slave_init())
        return false;
    bsp_i2c_slave_counters_t counters{};
    bsp_i2c_slave_get_counters(&counters);
    completion_count = counters.tx_complete_reads;
    recovery_failures = counters.recovery_init_failures;
    return true;
}

void app_panel_poll(uint32_t time_ms) {
    now_ms = time_ms;
    bsp_i2c_slave_counters_t counters{};
    bsp_i2c_slave_get_counters(&counters);
    if (counters.tx_complete_reads != completion_count) {
        completion_count = counters.tx_complete_reads;
        if (response_pending) {
            if (response_has_operation)
                app_recording_confirm(response_operation);
            if (response_has_reset)
                app_power_confirm_reset(now_ms);
            response_pending = false;
            ++confirmed_responses;
        }
    }
    const size_t received = bsp_i2c_slave_read(rx_buffer, sizeof(rx_buffer));
    if (received != 0U)
        panel_parser_push(parser, rx_buffer, received, now_ms, handle_frame, nullptr);
    panel_parser_poll(parser, now_ms);
    if (bsp_i2c_slave_poll(now_ms)) {
        panel_parser_reset_partial(parser); // 保留未读响应及其操作版本。
        bsp_i2c_slave_get_counters(&counters);
        if (counters.recovery_init_failures != recovery_failures)
            app_runtime_fault();
    }
}
