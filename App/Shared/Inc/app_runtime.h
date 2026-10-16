#ifndef APP_RUNTIME_H
#define APP_RUNTIME_H

#include <stdint.h>

struct AppRuntimeDiagnostics {
    uint32_t poll_count;
    uint32_t last_poll_ms;
    uint32_t max_poll_gap_ms;
    uint32_t max_work_ms;
};
/** @brief Runtime独占写入的HIL观测量；其他模块不得修改。 */
extern volatile AppRuntimeDiagnostics app_runtime_diagnostics;

/**
 * @brief 接管 Boot 留下的时钟、电源、时基和看门狗，成功后恢复全局中断。
 * @return 初始化失败返回 false，保持全局中断关闭；调用方必须进入终止故障路径。
 * @note 仅主循环启动时调用一次。要求 Boot 交接到 App 向量表及 200 MHz 晶振/MPLL 配置。
 */
bool app_runtime_init();
/** @brief 主循环每轮调用；服务看门狗、业务/LED与安全窗口参数保存，now_ms 允许回绕；Flash 阻塞预算待实测。 */
void app_runtime_poll(uint32_t now_ms);
/** @brief 停止喂狗、指示故障并驻留，不释放电源；可从初始化失败或异常上下文调用。 */
[[noreturn]] void app_runtime_fault();

#endif
