#include "bsp_power_policy.h"
#include "boot_log.h"
#include <assert.h>
void test_power_policy(void)
{
    bool modeled_latch = true;
    assert(bsp_power_policy_should_assert(0U));
    assert(bsp_power_policy_should_assert(UINT32_MAX));
    assert(modeled_latch);
    modeled_latch = bsp_power_policy_should_assert(0U);
    assert(modeled_latch);
    int expensive = 0;
    BOOT_LOG_INFO("TEST", "%d", ++expensive);
    assert(expensive == 0);
}
