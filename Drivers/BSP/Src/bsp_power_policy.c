#include "bsp_power_policy.h"
bool bsp_power_policy_should_assert(uint32_t raw_reset_flags) {
    (void)raw_reset_flags;
    return true;
}
