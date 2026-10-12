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
    uint32_t shutdown_started_ms;
    bool shutdown_pending, power_released;
    bool update_request_received;
    bool app_valid, watchdog_ready, led_ready, log_ready, jump_requested;
} boot_context_t;
void boot_capture_reset_info(boot_context_t* context);
void boot_select_mode(boot_context_t* context);
/* Main-loop only. Before the first supported request, update_started_ms is the I2C-ready time.
 * A software-reset update entry with no request for 1 s sets the completion token and resets.
 * Otherwise returns true once shutdown has started; the caller must stop accepting commands. */
bool boot_timeout_poll(boot_context_t* context, uint32_t now_ms);
#endif
