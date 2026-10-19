#ifndef BSP_INDICATOR_LED_H
#define BSP_INDICATOR_LED_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* X1 电源灯/充电灯；PB5 功能灯仍由 bsp_status_led 拥有，供 Boot 与异常路径共用。 */
typedef enum {
    BSP_INDICATOR_LED_POWER = 0, /* PC13 */
    BSP_INDICATOR_LED_CHARGE,    /* PH2 */
    BSP_INDICATOR_LED_COUNT
} bsp_indicator_led_t;
/** @brief 将 PC13/PH2 初始化为熄灭的推挽输出；须已解锁 GPIO 写保护，失败返回 false 且后续写入无操作。 */
bool bsp_indicator_led_init(void);
/** @brief 主循环输出逻辑亮/灭，内部处理有效电平；未初始化或越界编号时无操作。 */
void bsp_indicator_led_write(bsp_indicator_led_t led, bool on);
#ifdef __cplusplus
}
#endif
#endif
