#ifndef BOOT_STATE_H
#define BOOT_STATE_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { BOOT_MODE_RECOVERY, BOOT_MODE_UPDATE_WINDOW, BOOT_MODE_START_APPLICATION } boot_mode_t;
typedef struct {
    boot_mode_t mode;
    uint32_t reset_flags;
    uint32_t timeout_polls;
    bool jump_requested;
} boot_context_t;
void boot_early_init(boot_context_t* context);
void boot_select_mode(boot_context_t* context);
void boot_timeout_poll(boot_context_t* context);
#endif
