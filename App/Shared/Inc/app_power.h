#ifndef APP_POWER_H
#define APP_POWER_H
#include <stdint.h>

enum class AppPowerState { Starting, Running, ShutdownWait, ShutdownDelay, ResetWait, ResetReady, Off };
enum class AppPowerReason { None, Key, LowBattery, Linux, Forced, StartupTimeout };
struct AppPowerSnapshot {
    AppPowerState state;
    AppPowerReason reason;
    bool shutdown_requested;
};
/** @brief 主循环唯一所有者；cold_start 仅用于纯上电复位，继承 Boot 的 PB3 保持。 */
void app_power_init(bool cold_start, uint32_t now_ms);
/** @brief 推进截止时间和低电量策略，必要时释放 PB3/软件复位；不得在 ISR 调用。 */
void app_power_poll(uint32_t now_ms);
AppPowerSnapshot app_power_snapshot();
/** @brief 一次连续 2 s 电源长按：确认冷启动或发起关机，不延长已有截止时间。 */
void app_power_key_long_press(uint32_t now_ms);
/** @brief 功能键连续 8 s：立即释放 PB3，外部供电下驻留并继续服务看门狗。 */
void app_power_force_shutdown();
/** @brief 是否允许发布复位成功 ACK；只有 Running 状态可接受。 */
bool app_power_can_reset();
/** @brief 仅在复位 ACK 成功发布后调用，启动不可延长的 5 s 等待。 */
void app_power_request_reset(uint32_t now_ms);
/** @brief 仅关联的 ACK 完整读出且未超时才准备复位，迟读不复位。 */
void app_power_confirm_reset(uint32_t now_ms);
/** @brief Linux 0x05=1 为关机完成确认；成功 ACK 发布后调用，8 s 延时且最多总计 20 s。 */
bool app_power_can_confirm_shutdown();
void app_power_confirm_shutdown(uint32_t now_ms);
#endif
