#ifndef BSP_STATUS_LED_H
#define BSP_STATUS_LED_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/** @brief 初始化状态 LED GPIO 并熄灭；失败返回 false。 */
bool bsp_status_led_init(void);
/** @brief 输出逻辑亮/灭状态，内部处理板级有效电平；初始化失败时无操作。 */
void bsp_status_led_write(bool on);
bool bsp_status_led_is_ready(void);
/* Terminal fault only: drive an initialized LED on without a clock or scheduler. */
void bsp_status_led_fault_on(void);
#ifdef __cplusplus
}
#endif
#endif
