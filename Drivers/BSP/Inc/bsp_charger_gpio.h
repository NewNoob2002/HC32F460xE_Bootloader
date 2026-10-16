#ifndef BSP_CHARGER_GPIO_H
#define BSP_CHARGER_GPIO_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/** @brief 仅已识别V1.3调用：PA4使能/PB10电流选择初始化低，PA1/PB7/PB6输入上拉。 */
bool bsp_charger_gpio_init(void);
/** @brief Type-C原始检测：PA1高有效，依据旧HAL_Power.cpp；仅初始化成功后读取。 */
bool bsp_charger_gpio_typec_present(void);
/** @brief 旧充电状态原始输入PB7，保留与Type-C独立的观测。 */
bool bsp_charger_gpio_detect(void);
/** @brief PA4充电使能、PB10选择3A高电平；禁用时两者低。仅V1.3有效。 */
void bsp_charger_gpio_enable(bool enabled);
#ifdef __cplusplus
}
#endif
#endif
