#ifndef APP_PANEL_H
#define APP_PANEL_H
#include <stdint.h>

/** @brief 主循环初始化面板状态并接管 I2C1；调用时须关闭全局中断并解锁外设写保护。 */
bool app_panel_init();
/** @brief 有界处理接收、响应完成及恢复，ISR 只由 BSP 搬运字节。 */
void app_panel_poll(uint32_t now_ms);
/** @brief 主循环只读：本次启动至少收到过一帧 CRC/长度有效的 Linux 帧（包括暂不支持的消息）。 */
bool app_panel_linux_seen();
/** @brief 主循环只读：已发布但尚未完整读出的响应；恢复与短读不清除此状态，供 D7 保存门控。 */
bool app_panel_response_pending();

#endif
