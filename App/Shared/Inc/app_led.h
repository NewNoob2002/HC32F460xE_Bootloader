#ifndef APP_LED_H
#define APP_LED_H
#include <stdbool.h>
#include <stdint.h>
#include "app_power.h"

/** @brief 单路输出模式；翻转模式为每个间隔切换一次亮灭，进入模式时从亮开始。 */
enum class AppLedMode : uint8_t { Off, On, Toggle300Ms, Toggle500Ms, Toggle1000Ms, Toggle3000Ms };

/** @brief 只读输入，由调用方从电源/电池/录制快照与补偿后 SOC 映射得到。 */
struct AppLedInput {
    AppPowerState power;
    bool linux_seen; /* 录制闪烁门控：Linux 已出现过有效通信。 */
    bool recording;
    bool battery_valid; /* 新鲜且有效的 BQ 样本。 */
    bool charging;      /* 充电器有效且 charge_state 为 1 或 2（预充/快充）。 */
    bool fast_charging; /* charge_state == 2。 */
    bool charge_done;   /* 充电器有效且 charge_state == 3（终止）；接电期间会长期保持。 */
    uint8_t percent;    /* 补偿后 SOC，仅 battery_valid 时使用。 */
};

struct AppLedPattern {
    AppLedMode power;    /* PC13 */
    AppLedMode charge;   /* PH2 */
    AppLedMode function; /* PB5 */
};

/** @brief 纯函数：按 §5 优先级选择三灯模式，关机流程优先。 */
AppLedPattern app_led_select(const AppLedInput& input);

/** @brief 单路输出的时间状态，由调用方每个周期推进。 */
struct AppLedChannel {
    AppLedMode mode;
    uint32_t mode_started_ms;
};

/** @brief 返回 now_ms 时刻该路应驱动的逻辑电平；模式变化时从 now_ms 重新计相，支持 32 位毫秒回绕。 */
bool app_led_level(AppLedChannel& channel, AppLedMode mode, uint32_t now_ms);

#endif
