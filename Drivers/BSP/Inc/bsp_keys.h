#ifndef BSP_KEYS_H
#define BSP_KEYS_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/** @brief 初始化 X1 PA0 高有效电源键、PA15 低有效功能键；须已解锁 GPIO 并释放 JTAG 引脚。 */
bool bsp_keys_init(void);
/** @brief 主循环读取按下状态；消抖由 App 完成，不使用按键中断。 */
bool bsp_keys_power_pressed(void);
bool bsp_keys_function_pressed(void);
#ifdef __cplusplus
}
#endif
#endif
