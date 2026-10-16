#ifndef BOOT_RESET_H
#define BOOT_RESET_H
#include <stdbool.h>
#include <stdint.h>
/** @brief 记录一次性升级完成标记并请求软件复位；目标端不返回。 */
void boot_reset_after_upgrade(void);
/** @brief 消费保留 SRAM 标记；仅软件复位且标记完整时返回 true。 */
bool boot_reset_take_upgrade_marker(uint32_t reset_flags);
#endif
