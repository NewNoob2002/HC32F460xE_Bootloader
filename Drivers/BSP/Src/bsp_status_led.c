#include "bsp_status_led.h"
#include <stddef.h>
#include "boot_config.h"
#include "boot_timebase.h"

static led_scheduler_t board_led;
static bool board_ready;

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
    scheduler->active_high = active_high;
    scheduler->write = write;
    scheduler->write_context = context;
    led_scheduler_set_mode(scheduler, BOOT_LED_MODE_OFF, 0U);
}
void led_scheduler_set_mode(led_scheduler_t* scheduler, boot_led_mode_t mode, uint32_t now_ms) {
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
        scheduler->previous_ms = now_ms;
        scheduler->logical_on = !scheduler->logical_on;
        led_write(scheduler);
    }
}
bool bsp_status_led_init(void) {
#if BOOT_STATUS_LED_CONTRACT_CONFIRMED
    /* Populate only after legacy electrical polarity is supplied. */
#endif
    board_ready = false;
    return false;
}
void bsp_status_led_set_mode(boot_led_mode_t mode) {
    if (board_ready)
        led_scheduler_set_mode(&board_led, mode, boot_time_ms());
}
void bsp_status_led_poll(uint32_t now_ms) {
    if (board_ready)
        led_scheduler_poll(&board_led, now_ms);
}
void bsp_status_led_off(void) {
    if (board_ready)
        led_scheduler_set_mode(&board_led, BOOT_LED_MODE_OFF, boot_time_ms());
}
