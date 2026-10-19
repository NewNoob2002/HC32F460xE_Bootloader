#ifndef BSP_RESET_H
#define BSP_RESET_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint32_t bsp_reset_capture(void);
bool bsp_reset_was_software(uint32_t flags);
/** @brief 清除全部 RMU 复位标志；调用方须已解除 PWC/CLK/RMU 写保护。 */
void bsp_reset_clear(void);
/** @brief 普通软件复位，不写 Boot 升级完成标记；调用前完成响应及资源处理。 */
void bsp_reset_request(void);
#ifdef __cplusplus
}
#endif
#endif
