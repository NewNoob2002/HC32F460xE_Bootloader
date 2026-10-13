#include <assert.h>
#include <stdio.h>
#include "app_battery.h"
#include "app_keys.h"
#include "app_panel.h"
#include "app_power.h"
#include "app_runtime.h"
#include "bsp_debug_port.h"
#include "bsp_external_watchdog.h"
#include "bsp_indicator_led.h"
#include "bsp_power.h"
#include "bsp_reset.h"
#include "bsp_status_led.h"
#include "bsp_timebase.h"
#include "bsp_write_protection.h"
#include "hc32_ll.h"

namespace {
enum Stage { Power = 1, Hold, Debug, Clock, Pll, Xtal, Timebase, Led, Indicator, Watchdog, Battery, Panel, Keys };
unsigned stage;
unsigned fail_stage;
bool unlocked;
bool irq_masked;
unsigned watchdog_polls, key_polls, power_polls;
uint32_t reset_flags = RMU_FLAG_PWR_ON;
uint32_t now_ms;

bool next_stage(Stage expected) {
    assert(unlocked && irq_masked);
    ++stage;
    assert(stage == static_cast<unsigned>(expected));
    return stage != fail_stage;
}

void reset(unsigned failure = 0U, uint32_t now = 0U) {
    stage = 0U;
    fail_stage = failure;
    unlocked = false;
    irq_masked = false;
    watchdog_polls = key_polls = power_polls = 0U;
    now_ms = now;
}
} // namespace

extern "C" {
uint32_t SystemCoreClock;
uint32_t bsp_reset_capture(void) {
    assert(irq_masked && !unlocked && stage == Keys);
    return reset_flags;
}

bsp_irq_state_t bsp_enter_critical(void) {
    irq_masked = true;
    return 1U; // Boot 的继承状态。
}
void bsp_exit_critical(bsp_irq_state_t state) {
    assert(!unlocked && stage == Keys && state == 0U);
    irq_masked = false;
}
void bsp_write_protection_unlock(void) {
    assert(irq_masked);
    unlocked = true;
}
void bsp_write_protection_restore(void) {
    assert(irq_masked && unlocked);
    unlocked = false;
}
bool bsp_power_init(void) {
    return next_stage(Power);
}
bool bsp_power_hold_is_asserted(void) {
    return next_stage(Hold);
}
void bsp_debug_port_configure_for_gpio(void) {
    (void)next_stage(Debug);
}
void SystemCoreClockUpdate(void) {
    SystemCoreClock = next_stage(Clock) ? 200000000U : 16000000U;
}
en_flag_status_t CLK_GetStableStatus(uint8_t flag) {
    assert(flag == CLK_STB_FLAG_PLL || flag == CLK_STB_FLAG_XTAL);
    return next_stage(flag == CLK_STB_FLAG_PLL ? Pll : Xtal) ? SET : RESET;
}
bool bsp_timebase_init(void) {
    return next_stage(Timebase);
}
uint32_t bsp_time_ms(void) {
    return now_ms;
}
bool bsp_status_led_init(void) {
    return next_stage(Led);
}
bool bsp_indicator_led_init(void) {
    return next_stage(Indicator);
}
bool bsp_external_watchdog_init(uint32_t time) {
    assert(time == now_ms);
    return next_stage(Watchdog);
}
bool bsp_external_watchdog_is_pulse_active(void) {
    return false;
}
void bsp_external_watchdog_poll(uint32_t time) {
    assert(!irq_masked && !unlocked && time == now_ms);
    ++watchdog_polls;
}
} // extern "C"

bool app_battery_init() {
    return next_stage(Battery);
}
void app_battery_poll(uint32_t time) {
    assert(!irq_masked && !unlocked && time == now_ms);
}
bool app_panel_init() {
    return next_stage(Panel);
}
void app_panel_poll(uint32_t time) {
    assert(!irq_masked && !unlocked && time == now_ms);
}

bool app_keys_init(uint32_t time) {
    assert(time == now_ms);
    return next_stage(Keys);
}
void app_keys_poll(uint32_t time) {
    assert(!irq_masked && !unlocked && time == now_ms);
    ++key_polls;
}
void app_power_init(bool cold, uint32_t time) {
    assert(cold == (reset_flags == RMU_FLAG_PWR_ON));
    assert(irq_masked && !unlocked && time == now_ms);
}
void app_power_poll(uint32_t time) {
    assert(!irq_masked && !unlocked && time == now_ms);
    ++power_polls;
}

int main() {
    // 任一关键初始化失败都应短路、恢复写保护并保持中断关闭。
    const unsigned failures[] = {Power, Hold,      Clock,    Pll,     Xtal,  Timebase,
                                 Led,   Indicator, Watchdog, Battery, Panel, Keys};
    for (unsigned failure : failures) {
        reset(failure);
        assert(!app_runtime_init());
        assert(stage == failure && irq_masked && !unlocked && watchdog_polls == 0U);
    }
    const uint32_t flags_list[] = {RMU_FLAG_PWR_ON, 2U, 0x100U, RMU_FLAG_PWR_ON | 2U};
    for (uint32_t flags : flags_list) {
        reset_flags = flags;
        reset();
        assert(app_runtime_init());
    }
    reset(0U, UINT32_MAX - 500U);
    assert(app_runtime_init());
    assert(stage == Keys && !irq_masked && !unlocked);
    // 跨 uint32_t 回绕推进轮询；看门狗每轮均服务，心跳不能阻塞它。
    const uint32_t times[] = {UINT32_MAX - 1U, 0U, 499U, 500U, 1500U};
    for (uint32_t time : times) {
        now_ms = time;
        app_runtime_poll(now_ms);
    }
    assert(watchdog_polls == 5U && key_polls == 5U && power_polls == 10U);
    puts("x1_runtime_tests: init failures / IRQ order / wraparound PASS");
}
