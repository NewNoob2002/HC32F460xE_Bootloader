#include "app_runtime.h"
#include "bsp_charger_gpio.h"
#include "bsp_critical.h"
#include "bsp_external_watchdog.h"
#include "bsp_status_led.h"

[[noreturn]] void app_runtime_fault() {
    (void)bsp_enter_critical();
    // 保留 PB3 当前保持状态；不从异常路径改变尚未确认的产品供电策略。
    bsp_charger_gpio_enable(false);
    bsp_status_led_fault_on();
    bsp_external_watchdog_prepare_handover();
    for (;;) {}
}

// App 自有异常入口，不链接 Boot 的诊断状态与私有异常实现。
extern "C" void NMI_Handler() {
    app_runtime_fault();
}
extern "C" void HardFault_Handler() {
    app_runtime_fault();
}
extern "C" void MemManage_Handler() {
    app_runtime_fault();
}
extern "C" void BusFault_Handler() {
    app_runtime_fault();
}
extern "C" void UsageFault_Handler() {
    app_runtime_fault();
}
extern "C" void SVC_Handler() {
    app_runtime_fault();
}
extern "C" void DebugMon_Handler() {
    app_runtime_fault();
}
extern "C" void PendSV_Handler() {
    app_runtime_fault();
}
