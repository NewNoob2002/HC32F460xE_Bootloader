#include "bsp_external_watchdog.h"
#include <stddef.h>
#include "boot_config.h"
#include "boot_timebase.h"

static watchdog_scheduler_t board_scheduler;
static bool board_ready;

void watchdog_scheduler_init(watchdog_scheduler_t* scheduler, uint32_t now_ms, uint32_t interval_ms, uint32_t pulse_ms,
                             bool active_level, watchdog_gpio_write_t write, void* context) {
    scheduler->state = WATCHDOG_FEED_STATE_IDLE;
    scheduler->interval_ms = interval_ms;
    scheduler->pulse_ms = pulse_ms;
    scheduler->next_feed_ms = now_ms + interval_ms;
    scheduler->pulse_end_ms = now_ms;
    scheduler->active_level = active_level;
    scheduler->enabled = (write != NULL) && (interval_ms != 0U) && (pulse_ms != 0U) && (pulse_ms <= interval_ms);
    scheduler->write = write;
    scheduler->write_context = context;
    if (scheduler->enabled)
        scheduler->write(!active_level, context);
}

void watchdog_scheduler_force(watchdog_scheduler_t* scheduler, uint32_t now_ms) {
    if ((scheduler == NULL) || !scheduler->enabled || (scheduler->state != WATCHDOG_FEED_STATE_IDLE))
        return;
    scheduler->write(scheduler->active_level, scheduler->write_context);
    scheduler->state = WATCHDOG_FEED_STATE_PULSE_ACTIVE;
    scheduler->pulse_end_ms = now_ms + scheduler->pulse_ms;
    scheduler->next_feed_ms = now_ms + scheduler->interval_ms;
}

void watchdog_scheduler_poll(watchdog_scheduler_t* scheduler, uint32_t now_ms) {
    if ((scheduler == NULL) || !scheduler->enabled)
        return;
    if (scheduler->state == WATCHDOG_FEED_STATE_IDLE) {
        uint32_t late_ms;
        uint32_t periods;
        if ((int32_t)(now_ms - scheduler->next_feed_ms) < 0)
            return;
        scheduler->write(scheduler->active_level, scheduler->write_context);
        scheduler->state = WATCHDOG_FEED_STATE_PULSE_ACTIVE;
        scheduler->pulse_end_ms = now_ms + scheduler->pulse_ms;
        late_ms = now_ms - scheduler->next_feed_ms;
        periods = (late_ms / scheduler->interval_ms) + 1U;
        scheduler->next_feed_ms += periods * scheduler->interval_ms;
    } else {
        if ((int32_t)(now_ms - scheduler->pulse_end_ms) < 0)
            return;
        scheduler->write(!scheduler->active_level, scheduler->write_context);
        scheduler->state = WATCHDOG_FEED_STATE_IDLE;
    }
}

bool bsp_external_watchdog_init(void) {
#if BOOT_EXTERNAL_WATCHDOG_CONTRACT_CONFIRMED
    /* Populate only after legacy polarity and pulse width are supplied. */
#endif
    board_ready = false;
    return false;
}
void bsp_external_watchdog_poll(uint32_t now_ms) {
    if (board_ready)
        watchdog_scheduler_poll(&board_scheduler, now_ms);
}
void bsp_external_watchdog_force_feed(void) {
    if (board_ready)
        watchdog_scheduler_force(&board_scheduler, boot_time_ms());
}
bool bsp_external_watchdog_is_ready(void) {
    return board_ready;
}
