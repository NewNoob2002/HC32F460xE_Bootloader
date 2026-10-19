#ifndef APP_KEYS_H
#define APP_KEYS_H
#include <stdint.h>
/** @brief 主循环初始化 MultiButton 和按键 BSP；GPIO 写保护须已解锁。 */
bool app_keys_init(uint32_t now_ms);
/** @brief 每轮调用；MultiButton 最多每 5 ms 一次，不用同一 GPIO 样本补跑错过的 tick。
 * 单/双击、消抖由库处理；产品长按使用实际时间，一段持续按下只触发一次。
 */
void app_keys_poll(uint32_t now_ms);
#endif
