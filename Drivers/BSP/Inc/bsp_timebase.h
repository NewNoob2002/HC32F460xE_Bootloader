#ifndef BSP_TIMEBASE_H
#define BSP_TIMEBASE_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** @brief 配置独占的 1 ms SysTick；失败返回 false。与 RTOS SysTick 二选一。 */
bool bsp_timebase_init(void);
/** @brief 返回自初始化起的毫秒数，允许 uint32_t 回绕。 */
uint32_t bsp_time_ms(void);
/** @brief 停止并清除 SysTick，用于镜像交接。 */
void bsp_timebase_deinit(void);
static inline bool bsp_time_elapsed(uint32_t now, uint32_t previous, uint32_t interval) {
    return (uint32_t)(now - previous) >= interval;
}
#ifdef __cplusplus
}
#endif
#endif
