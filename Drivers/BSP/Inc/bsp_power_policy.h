#ifndef BSP_POWER_POLICY_H
#define BSP_POWER_POLICY_H
#include <stdbool.h>
#include <stdint.h>
bool bsp_power_policy_should_assert(uint32_t raw_reset_flags);
#endif
