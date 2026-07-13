#ifndef BOOT_TIMEBASE_H
#define BOOT_TIMEBASE_H
#include <stdbool.h>
#include <stdint.h>
bool boot_timebase_init(void);
uint32_t boot_time_ms(void);
void boot_timebase_deinit(void);
static inline bool boot_time_elapsed(uint32_t now, uint32_t previous, uint32_t interval)
{
    return (uint32_t)(now - previous) >= interval;
}
#endif
