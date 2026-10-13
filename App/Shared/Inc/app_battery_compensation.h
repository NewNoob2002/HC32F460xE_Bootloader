#ifndef APP_BATTERY_COMPENSATION_H
#define APP_BATTERY_COMPENSATION_H
#include <stdbool.h>
#include <stdint.h>

/** @brief 充电末端 SOC 补偿状态（x1_p5_plan D1/D2/D4）；纯逻辑，持久化由调用方完成。 */
struct AppSocCompensation {
    uint32_t stable_since_ms;
    uint16_t stable_soc;
    uint16_t rejected; /* 诊断：计算出 offset≥10 而被丢弃的次数。 */
    uint8_t offset;    /* 生效偏移，始终在 [0,9]。 */
    bool timing;
    bool session_done; /* 本次充电会话已得出结果；停止充电后清除。 */
    bool save_pending; /* 新偏移待保存；调用方保存成功后清除。 */
};

/** @brief 以已保存的偏移初始化；超出 [0,9] 视为损坏，按 0 处理。 */
void app_soc_compensation_init(AppSocCompensation& comp, uint8_t stored_offset);
/**
 * @brief 周期推进。charging 包含预充/快充/终止（MP2762 state 1–3）；
 * 仅有效且未陈旧的样本推进计时，原始 SOC 不变满 20 min 得到 offset=100-raw。
 */
void app_soc_compensation_update(AppSocCompensation& comp, bool sample_valid, uint16_t raw_soc, bool charging,
                                 uint32_t now_ms);
/** @brief 补偿后 SOC：clamp(raw+offset, 0, 100)。 */
uint8_t app_soc_compensation_percent(const AppSocCompensation& comp, uint16_t raw_soc);
#endif
