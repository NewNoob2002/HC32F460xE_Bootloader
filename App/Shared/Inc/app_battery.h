#ifndef APP_BATTERY_H
#define APP_BATTERY_H
#include <stdint.h>
#include "bq40z50.h"

enum class AppHardware { Unknown, GpioV13, Mp2762V15Plus };
struct AppBatterySnapshot {
    AppHardware hardware;
    Bq40z50Sample sample;
    uint32_t sampled_ms;
    bool valid;
    bool charger_valid;
    bool typec_present;
    bool charging_enabled;
    uint8_t charge_state;
    uint8_t percent; /* 补偿后 SOC，供面板与 LED（D3）；低电量判断继续用 sample.soc_percent。 */
};
/** @brief 初始化总线和状态，加载已读取的 SOC 偏移（[0,9]，越界按 0）；版本识别前不触碰旧版充电 GPIO。 */
bool app_battery_init(uint8_t stored_offset = 0U);
/** @brief 主循环周期处理；不要在看门狗高脉冲期间启动同步I2C2操作。 */
void app_battery_poll(uint32_t now_ms);
/** @brief 主循环在 D7 安全窗口保存待写偏移；仅成功后清除 pending，失败最早 3 s 后重试，now_ms 允许回绕。 */
void app_battery_save_compensation(uint32_t now_ms);
/** @brief 复制只读快照并根据now_ms判定陈旧，不将失败/陈旧样本发布为有效。 */
AppBatterySnapshot app_battery_snapshot(uint32_t now_ms);
/** @brief 从已锁定方案得到线缆硬件版本；V1.5+只代表充电方案，不识别更细的PCB修订。 */
const char* app_hardware_name(AppHardware hardware);
#endif
