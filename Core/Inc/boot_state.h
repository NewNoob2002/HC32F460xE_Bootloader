#ifndef BOOT_STATE_H
#define BOOT_STATE_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { BOOT_MODE_RECOVERY, BOOT_MODE_UPDATE_WINDOW, BOOT_MODE_START_APPLICATION } boot_mode_t;
typedef struct {
    uint32_t raw_flags;
    bool power_on_reset, pin_reset, software_reset, watchdog_reset;
    bool low_voltage_reset, clock_failure_reset, multiple_reset_sources;
} boot_reset_info_t;
typedef struct {
    boot_mode_t mode;
    boot_reset_info_t reset_info;
    uint32_t update_started_ms;
    bool app_valid, watchdog_ready, led_ready, log_ready, jump_requested;
} boot_context_t;
void boot_capture_reset_info(boot_context_t* context);
void boot_select_mode(boot_context_t* context);
void boot_timeout_poll(boot_context_t* context, uint32_t now_ms);
#endif
