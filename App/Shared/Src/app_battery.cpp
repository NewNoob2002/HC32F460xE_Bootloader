#define LOG_TAG "Battery"
#include "app_battery.h"
#include "app_battery_compensation.h"
#include "app_param_store.h"
#include "bsp_charger_gpio.h"
#include "bsp_i2c_master.h"
#include "bsp_timebase.h"
#include "firmware_log.h"
#include "mp2762a.h"

namespace {
AppBatterySnapshot battery{};
AppSocCompensation compensation{};
uint32_t last_sample_ms;
uint32_t last_control_ms;
uint32_t plugged_ms;
uint32_t read_failures;
uint32_t charger_failures;
uint32_t save_failures;
uint32_t last_save_ms;
uint8_t absent_confirmations;
bool sample_started;
bool control_started;
bool plug_started;
bool charger_initialized;
bool save_attempted;

void identify_hardware(bool gauge_healthy) {
    if (battery.hardware != AppHardware::Unknown)
        return;
    const auto probe = mp2762_probe();
    if (probe == BSP_I2C_PROBE_PRESENT) {
        battery.hardware = AppHardware::Mp2762V15Plus;
    } else if (gauge_healthy && (probe == BSP_I2C_PROBE_ADDRESS_NACK)) {
        // ponytail: 三次健康总线NACK识别旧拓扑；首次完全失联的MP需可靠出厂标识排除。
        if (++absent_confirmations >= 3U)
            battery.hardware = AppHardware::GpioV13;
    } else {
        absent_confirmations = 0U;
    }
    if (battery.hardware != AppHardware::Unknown)
        FIRMWARE_LOG_INFO("hardware=%s mode=%u", app_hardware_name(battery.hardware),
                          static_cast<unsigned>(battery.hardware));
}

void stop_mp_on_error() {
    ++charger_failures;
    if (!mp2762_set_charging(false) && (charger_failures == 1U)) {
        FIRMWARE_LOG_ERROR("MP2762 communication failed; charge disable unconfirmed");
    }
    battery.charging_enabled = false;
}

void update_charger(uint32_t now_ms) {
    battery.charger_valid = false;
    if (!charger_initialized) {
        if (battery.hardware == AppHardware::GpioV13)
            charger_initialized = bsp_charger_gpio_init();
        else if (battery.hardware == AppHardware::Mp2762V15Plus)
            charger_initialized = mp2762_configure();
        if (!charger_initialized)
            return;
    }
    const bool safe_temperature = battery.valid && !bsp_time_elapsed(now_ms, battery.sampled_ms, 6000U)
                                  && (battery.sample.temperature_dk >= 2732U)
                                  && (battery.sample.temperature_dk < 3182U); // 不在0°C以下充电，旧产品上限45°C。
    if (battery.hardware == AppHardware::GpioV13) {
        battery.typec_present = bsp_charger_gpio_typec_present();
        const bool source_present = battery.typec_present && bsp_charger_gpio_detect();
        if (!source_present) {
            plug_started = false;
        } else if (!plug_started) {
            plug_started = true;
            plugged_ms = now_ms;
        }
        const bool enable = safe_temperature && source_present && bsp_time_elapsed(now_ms, plugged_ms, 1500U);
        bsp_charger_gpio_enable(enable);
        battery.charging_enabled = enable;
        battery.charge_state = enable ? 2U : 0U;
        battery.charger_valid = true;
    } else {
        Mp2762Status status{};
        if (!mp2762_read_status(status)) {
            stop_mp_on_error();
            return;
        }
        const bool enable = safe_temperature && status.input_present;
        if (!mp2762_set_charging(enable) || !mp2762_read_status(status)) {
            stop_mp_on_error();
            return;
        }
        battery.typec_present = status.input_present;
        battery.charging_enabled = enable;
        battery.charge_state = status.charge_state;
        battery.charger_valid = true;
    }
}
} // namespace

bool app_battery_init(uint8_t stored_offset) {
    battery = {};
    app_soc_compensation_init(compensation, stored_offset);
    save_attempted = false;
    sample_started = false;
    control_started = false;
    plug_started = false;
    charger_initialized = false;
    absent_confirmations = 0U;
    read_failures = 0U;
    charger_failures = 0U;
    save_failures = 0U;
    return bsp_i2c_master_init();
}

void app_battery_poll(uint32_t now_ms) {
    bool sample_taken = false;
    const uint32_t sample_interval = battery.hardware == AppHardware::Unknown ? 200U : 3000U;
    if (!sample_started || bsp_time_elapsed(now_ms, last_sample_ms, sample_interval)) {
        sample_started = true;
        sample_taken = true;
        last_sample_ms = now_ms;
        Bq40z50Sample sample{};
        battery.valid = bq40z50_read_sample(sample);
        if (battery.valid) {
            battery.sample = sample;
            battery.sampled_ms = now_ms;
        } else {
            ++read_failures;
        }
        identify_hardware(battery.valid);
        if (battery.valid) {
            FIRMWARE_LOG_DEBUG("soc=%u voltage_mv=%u temp_dk=%u", static_cast<unsigned>(sample.soc_percent),
                               static_cast<unsigned>(sample.voltage_mv), static_cast<unsigned>(sample.temperature_dk));
        }
    }
    if (!control_started || sample_taken || bsp_time_elapsed(now_ms, last_control_ms, 500U)) {
        control_started = true;
        last_control_ms = now_ms;
        update_charger(now_ms);
        // 终止态 3 也计入：V1.5 终止后原始 SOC 常停在 98–99%，正是需补偿的场景。
        app_soc_compensation_update(compensation, battery.valid && !bsp_time_elapsed(now_ms, battery.sampled_ms, 6000U),
                                    battery.sample.soc_percent, battery.charger_valid && (battery.charge_state != 0U),
                                    now_ms);
    }
}

void app_battery_save_compensation(uint32_t now_ms) {
    if (!compensation.save_pending || (save_attempted && !bsp_time_elapsed(now_ms, last_save_ms, 3000U)))
        return;
    save_attempted = true;
    last_save_ms = now_ms;
    if (app_param_store_save(compensation.offset)) {
        compensation.save_pending = false;
    } else if (++save_failures == 1U) {
        FIRMWARE_LOG_WARN("SOC offset save failed; keeping pending value for retry");
    }
}

AppBatterySnapshot app_battery_snapshot(uint32_t now_ms) {
    auto result = battery;
    result.valid = result.valid && !bsp_time_elapsed(now_ms, result.sampled_ms, 6000U);
    result.percent = app_soc_compensation_percent(compensation, result.sample.soc_percent);
    return result;
}

const char* app_hardware_name(AppHardware hardware) {
    if (hardware == AppHardware::GpioV13)
        return "V1.3";
    if (hardware == AppHardware::Mp2762V15Plus)
        return "V1.5";
    return "UNKNOWN";
}
