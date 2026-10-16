#include "boot_status_led.h"
#include <stddef.h>
#include "bsp_timebase.h"

static uint32_t mode_interval(boot_led_mode_t mode) {
    if (mode == BOOT_LED_MODE_UPDATE_WINDOW)
        return 250U;
    if (mode == BOOT_LED_MODE_RECOVERY)
        return 1000U;
    if (mode == BOOT_LED_MODE_FATAL)
        return 100U;
    return 0U;
}
static void led_write(led_scheduler_t* scheduler) {
    if (scheduler->write != NULL)
        scheduler->write(scheduler->logical_on == scheduler->active_high, scheduler->write_context);
}
void led_scheduler_init(led_scheduler_t* scheduler, bool active_high, led_gpio_write_t write, void* context) {
    scheduler->mode = BOOT_LED_MODE_OFF;
    scheduler->previous_ms = 0U;
    scheduler->interval_ms = 0U;
    scheduler->logical_on = false;
    scheduler->active_high = active_high;
    scheduler->write = write;
    scheduler->write_context = context;
    led_write(scheduler);
}
void led_scheduler_set_mode(led_scheduler_t* scheduler, boot_led_mode_t mode, uint32_t now_ms) {
    if (scheduler->mode == mode)
        return;
    scheduler->mode = mode;
    scheduler->previous_ms = now_ms;
    scheduler->interval_ms = mode_interval(mode);
    scheduler->logical_on = mode == BOOT_LED_MODE_BOOTING;
    led_write(scheduler);
}
void led_scheduler_poll(led_scheduler_t* scheduler, uint32_t now_ms) {
    if ((scheduler == NULL) || (scheduler->interval_ms == 0U))
        return;
    if ((uint32_t)(now_ms - scheduler->previous_ms) >= scheduler->interval_ms) {
        uint32_t elapsed = now_ms - scheduler->previous_ms;
        uint32_t periods = elapsed / scheduler->interval_ms;
        scheduler->previous_ms += periods * scheduler->interval_ms;
        if ((periods & 1U) != 0U)
            scheduler->logical_on = !scheduler->logical_on;
        led_write(scheduler);
    }
}

#if !defined(HC32_HOST_TEST)
#include "bsp_status_led.h"

static led_scheduler_t board_led;

static void board_led_write(bool on, void* context) {
    (void)context;
    bsp_status_led_write(on);
}
bool boot_status_led_init(void) {
    if (!bsp_status_led_init())
        return false;
    /* 调度器输出逻辑亮/灭；有效电平由共用 BSP 处理。 */
    led_scheduler_init(&board_led, true, board_led_write, NULL);
    return true;
}
void boot_status_led_set_mode(boot_led_mode_t mode) {
    if (bsp_status_led_is_ready())
        led_scheduler_set_mode(&board_led, mode, bsp_time_ms());
}
void boot_status_led_poll(uint32_t now_ms) {
    if (bsp_status_led_is_ready())
        led_scheduler_poll(&board_led, now_ms);
}
void boot_status_led_off(void) {
    if (bsp_status_led_is_ready())
        led_scheduler_set_mode(&board_led, BOOT_LED_MODE_OFF, bsp_time_ms());
}
#endif
